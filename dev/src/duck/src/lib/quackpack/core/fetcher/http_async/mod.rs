//! An async HTTP client, backed by [`curl`] and [`futures`].
//!
//! It uses a helper thread to achieve asynchrony.
//!
//! ## Lifetime and usage of the main client:
//! * [`new`](HttpAsyncClient::new) creates a helper thread and a synchronous
//!   [`channel`](sync_mpsc::channel) for communication. The thread gets a receiver, while client
//!   keeps a sender.
//! * when performing a request, client creates an [`Easy2`] and an asynchronous
//!   [`oneshot::channel`](async_oneshot::channel). Then it uses its sender to send an [`Easy2`] and
//!   an associated async [`Sender`](async_oneshot::Sender) to the helper thread. After that, it
//!   `await`s on the receiving half, to get an HTTP response.
//!
//! ## Lifetime of the helper thread
//! In a loop, the thread performs these steps:
//! * receive and enqueue __all__ pending connections, and insert them into a [`Multi`],
//! * drive [`Multi`], using [`perform`](Multi::perform), [`messages`](Multi::messages), and
//!   [`wait`](Multi::messages), and process completed/failed requests,
//! * [optionally] check for a closed channel, and if it was, stop.
//!
//! We also always set [`token`](Easy2Handle::set_token) to track new requests.

use std::collections::HashMap;
use std::ops::ControlFlow;
use std::sync::mpsc as sync_mpsc;
use std::thread::{self, JoinHandle};
use std::time::Duration;

use curl::easy::{Easy2, Handler};
use curl::multi::{Easy2Handle, Multi};
use futures::channel::oneshot as async_oneshot;
use tracing::{error, info, trace};

use super::util::http::handlers::Collector;
use super::util::http::{
    Request, Response, check_http_status_code, configure_easy2, response_from_handler,
};
use crate::{DuckContext, QuackResult, QuackResultContext};

struct IncomingConnectionRequest {
    handler: Easy2<Collector>,
    sender: async_oneshot::Sender<CompletedRequest>,
}

struct CompletedRequest {
    response: QuackResult<Response>,
}

#[derive(Debug)]
/// Our implementation of a curl-backed async HTTP client.
pub struct AsyncHttpClient<'duck> {
    ctx: &'duck DuckContext,
    // NOTE: These two members are wrapped in an `Option`, because:
    // 1. in `Drop` we firstly need to close a channel;
    //    * which requires `drop(sender)`,
    //    * but drop on a mutable reference does nothing, therefore we need to take `sender` by value,
    // 2. then we need to join the thread;
    //    * but the `join()` takes handle by value.
    //
    // Since `drop` has `&mut self` in signature, we wrap these two fields in `Option`s, which are
    // always `Some`, and use `.take()`.
    /// A sender used to send requests to a worker thread.
    sender: Option<sync_mpsc::Sender<IncomingConnectionRequest>>,
    /// A handle to the worker thread.
    worker: Option<JoinHandle<()>>,
}

impl<'duck> AsyncHttpClient<'duck> {
    /// Construct a new [`AsyncHttpClient`].
    pub fn new(ctx: &'duck DuckContext) -> Self {
        let (sender, receiver) = sync_mpsc::channel();
        let thread = thread::spawn(move || {
            let mut worker = Worker::new(receiver);
            worker.run_loop();
        });
        Self {
            ctx,
            sender: Some(sender),
            worker: Some(thread),
        }
    }

    /// Perform an asynchronous generic HTTP request.
    #[tracing::instrument(skip_all)]
    pub async fn request(&self, request: Request) -> QuackResult<Response> {
        let mut easy = self.create_easy(Collector::default(), &request)?;
        let (parts, body) = request.into_parts();
        if parts.method == http::Method::POST {
            easy.get_mut().set_request_body(body);
        }
        let (sender, receiver) = async_oneshot::channel();
        let request = IncomingConnectionRequest {
            handler: easy,
            sender,
        };
        self.sender
            .as_ref()
            .expect("always Some()")
            .send(request)
            .expect("receiver is closed after sender");
        let response = receiver
            .await
            .expect("we don't cancel futures")
            .response
            .context("failed to execute an HTTP request")?;
        check_http_status_code(&response, &parts.uri)?;
        Ok(response)
    }

    /// Create a common [`Easy2`] handler.
    fn create_easy<H: Handler>(&self, handler: H, request: &Request) -> QuackResult<Easy2<H>> {
        let mut easy = Easy2::new(handler);
        configure_easy2(&mut easy, self.ctx, request)?;
        Ok(easy)
    }
}

impl Drop for AsyncHttpClient<'_> {
    fn drop(&mut self) {
        drop(self.sender.take().unwrap());
        let _ = self.worker.take().unwrap().join();
    }
}

#[derive(Clone, Copy)]
enum ShouldCheckForClosedChannel {
    No,
    Yes,
}

impl ShouldCheckForClosedChannel {
    fn should_check_for_closed_channel(self) -> bool {
        matches!(self, Self::Yes)
    }
}

struct Worker {
    /// Receiver used to receive requests from the main client/thread.
    receiver: sync_mpsc::Receiver<IncomingConnectionRequest>,
    /// [`Multi`] used for performing asynchronous requests.
    multi: Multi,
    /// All pending/unprocessed requests/connections.
    connections: HashMap<
        usize,
        (
            Easy2Handle<Collector>,
            async_oneshot::Sender<CompletedRequest>,
        ),
    >,
    /// Next available token for the [`set_token`](Easy2Handle::set_token).
    next_token: usize,
}

impl Worker {
    /// Create a new [`Worker`] instance.
    fn new(receiver: sync_mpsc::Receiver<IncomingConnectionRequest>) -> Self {
        let mut multi = Multi::new();
        if let Err(e) = multi.pipelining(false, true) {
            error!("failed to set pipelining on multi: {e}");
        }
        Self {
            receiver,
            multi,
            connections: HashMap::default(),
            next_token: 0,
        }
    }

    /// Run continuously worker, until the main client is dropped.
    fn run_loop(&mut self) {
        loop {
            if self.run_single_loop_iteration().is_break() {
                return;
            }
        }
    }

    /// Execute a single iteration of the worker's work.
    fn run_single_loop_iteration(&mut self) -> ControlFlow<()> {
        self.enqueue_incoming_connections();
        let should_check_for_closed_channel = self.handle_completed_connections();
        if should_check_for_closed_channel.should_check_for_closed_channel() {
            return self.check_for_closed_channel();
        }
        ControlFlow::Continue(())
    }

    /// Enqueue all new incoming/pending connections.
    fn enqueue_incoming_connections(&mut self) {
        // Use a non-blocking receiver, so that we don't stop processing requests while waiting for
        // new requests.
        while let Ok(request) = self.receiver.try_recv() {
            self.enqueue_incoming_connection(request);
        }
    }

    /// Enqueue a single new connection. This will add an [`Easy2`] into a [`Multi`], and also into
    /// an internal [`HashMap`].
    ///
    /// On failure, we send back an error.
    fn enqueue_incoming_connection(
        &mut self,
        IncomingConnectionRequest { handler, sender }: IncomingConnectionRequest,
    ) {
        match self.register_new_handler_in_multi(handler) {
            Ok((token, handle)) => {
                self.connections.insert(token, (handle, sender));
            }
            Err(err) => {
                error!("failed to register a new Easy2Handle: {err}");
                let _ = sender.send(CompletedRequest { response: Err(err) });
            }
        };
    }

    /// Register new [`Easy2`] handler in [`Multi`]. This method, unfortunately, can fail.
    fn register_new_handler_in_multi(
        &mut self,
        handler: Easy2<Collector>,
    ) -> QuackResult<(usize, Easy2Handle<Collector>)> {
        let mut handle = self
            .multi
            .add2(handler)
            .context("failed to add a handle to a multi")?;
        let token = self.next_token;
        self.next_token += 1;
        handle.set_token(token)?;
        Ok((token, handle))
    }

    /// Drive [`Multi`] and process completed requests.
    fn handle_completed_connections(&mut self) -> ShouldCheckForClosedChannel {
        const DEFAULT_DELAY: Duration = Duration::from_millis(100);
        // Firstly, try to poll pending requests.
        let transfers_in_progress = match self.multi.perform() {
            Ok(transfers_in_progress) => transfers_in_progress,
            Err(err) => {
                // We should call `perform` again.
                if err.is_call_perform() {
                    trace!("got `is_call_perform`");
                    return ShouldCheckForClosedChannel::No;
                }
                error!("failed to perform");
                self.fail_all_connections(&err);
                return ShouldCheckForClosedChannel::No;
            }
        };
        // Secondly, process completed requests.
        self.multi.messages(|msg| {
            let token = msg.token().expect("we set token on all handles");
            let Some((handle, sender)) = self.connections.remove(&token) else {
                return;
            };
            let handler_result = msg.result_for2(&handle).expect("we only have one multi");
            let handler = self.multi.remove2(handle).expect("we have only one multi");
            let response = response_from_handler(handler);
            let response = handler_result.map(|_| response).map_err(Into::into);
            let _ = sender.send(CompletedRequest { response });
        });
        // There are still pending transfers. Wait a bit.
        if transfers_in_progress > 0 {
            trace!("still has work to do");
            let delay = self
                .multi
                .get_timeout()
                .ok()
                .flatten()
                .unwrap_or(DEFAULT_DELAY);
            if let Err(e) = self.multi.wait(&mut [], delay) {
                error!("failed to wait");
                self.fail_all_connections(&e);
            }
            return ShouldCheckForClosedChannel::No;
        }
        // All work has been done. Check, if the pipe hasn't been closed.
        ShouldCheckForClosedChannel::Yes
    }

    /// Check, if the communication channel between threads has been closed (which means that the
    /// [`AsyncHttpClient`] is being dropped, and waits for us to finish work; process all pending
    /// requests, and then return).
    fn check_for_closed_channel(&mut self) -> ControlFlow<()> {
        match self.receiver.recv() {
            Ok(request) => {
                trace!("resumed work");
                self.enqueue_incoming_connection(request);
                ControlFlow::Continue(())
            }
            // Pipe has been closed. Shut down worker.
            Err(_) => {
                info!("shutting down");
                ControlFlow::Break(())
            }
        }
    }

    /// We've got an error when driving a [`Multi`]. Close all opened
    fn fail_all_connections(&mut self, error: &curl::MultiError) {
        error!("stopping all pending HTTP connections because got error: {error}");
        // NOTE: Handles are detached/removed when dropped.
        for (_, (_, sender)) in self.connections.drain() {
            let _ = sender.send(CompletedRequest {
                response: Err(error.clone()).context("failed to execute curl multi"),
            });
        }
    }
}
