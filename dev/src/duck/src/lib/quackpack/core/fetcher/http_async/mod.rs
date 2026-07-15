//! General HTTP client, backed by [`curl`].

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
pub struct HttpAsyncClient<'duck> {
    ctx: &'duck DuckContext,
    // NOTE: These two members are wrapped in an `Option`, because:
    // 1. in `Drop` we firstly need to close a channel;
    //    * which requires `drop(tx)`,
    //    * but drop on a mutable reference does nothing, therefore we need to take `tx` by value,
    // 2. then we need to join the thread;
    //    * but the `join()` takes handle by value.
    //
    // Since `drop` has `&mut self` in signature, we wrap these two fields in `Option`s, which are
    // always `Some`, and use `.take()`.
    tx: Option<sync_mpsc::Sender<IncomingConnectionRequest>>,
    worker: Option<JoinHandle<()>>,
}

impl<'duck> HttpAsyncClient<'duck> {
    /// Construct a new [`HttpClient`].
    pub fn new(ctx: &'duck DuckContext) -> Self {
        let (tx, rx) = sync_mpsc::channel();
        let thread = thread::spawn(move || {
            let mut worker = Worker::new(rx);
            worker.run();
        });
        Self {
            ctx,
            tx: Some(tx),
            worker: Some(thread),
        }
    }

    /// Create a common [`Easy2`] handler.
    fn create_easy<H: Handler>(&self, handler: H, request: &Request) -> QuackResult<Easy2<H>> {
        let mut easy = Easy2::new(handler);
        configure_easy2(&mut easy, self.ctx, request)?;
        Ok(easy)
    }

    /// Perform a generic HTTP request.
    #[tracing::instrument(skip_all)]
    pub async fn request(&self, request: Request) -> QuackResult<Response> {
        let mut easy = self.create_easy(Collector::default(), &request)?;
        let (parts, body) = request.into_parts();
        if parts.method == http::Method::POST {
            easy.get_mut().set_request_body(body);
        }
        let (tx, rx) = async_oneshot::channel();
        let request = IncomingConnectionRequest {
            handler: easy,
            sender: tx,
        };
        self.tx
            .as_ref()
            .expect("always Some()")
            .send(request)
            .expect("receiver is closed after sender");
        let response = rx
            .await
            .expect("we don't cancel futures")
            .response
            .context("failed to execute an HTTP request")?;
        check_http_status_code(&response, &parts.uri)?;
        Ok(response)
    }
}

impl Drop for HttpAsyncClient<'_> {
    fn drop(&mut self) {
        drop(self.tx.take().unwrap());
        let _ = self.worker.take().unwrap().join();
    }
}

struct Worker {
    rx: sync_mpsc::Receiver<IncomingConnectionRequest>,
    multi: Multi,
    connections: HashMap<
        usize,
        (
            Easy2Handle<Collector>,
            async_oneshot::Sender<CompletedRequest>,
        ),
    >,
    next_token: usize,
}

impl Worker {
    fn new(rx: sync_mpsc::Receiver<IncomingConnectionRequest>) -> Self {
        let mut multi = Multi::new();
        if let Err(e) = multi.pipelining(false, true) {
            error!("failed to set pipelining on multi: {e}");
        }
        Self {
            rx,
            multi,
            connections: HashMap::default(),
            next_token: 0,
        }
    }

    fn run(&mut self) {
        loop {
            self.enqueue_incoming_connections();
            if self.handle_completed_connections().is_break() {
                return;
            }
        }
    }

    fn enqueue_incoming_connections(&mut self) {
        while let Ok(request) = self.rx.try_recv() {
            self.enqueue_incoming_connection(request);
        }
    }

    fn enqueue_incoming_connection(
        &mut self,
        IncomingConnectionRequest { handler, sender }: IncomingConnectionRequest,
    ) {
        match self.enqueue_new_handler(handler) {
            Ok((token, handle)) => {
                self.connections.insert(token, (handle, sender));
            }
            Err(err) => {
                let _ = sender.send(CompletedRequest { response: Err(err) });
            }
        };
    }

    fn enqueue_new_handler(
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

    fn handle_completed_connections(&mut self) -> ControlFlow<()> {
        const DEFAULT_DELAY: Duration = Duration::from_millis(100);
        // Firstly, try to poll pending requests.
        let transfers_in_progress = match self.multi.perform() {
            Ok(transfers_in_progress) => transfers_in_progress,
            Err(err) => {
                // We should call `perform` again.
                if err.is_call_perform() {
                    return ControlFlow::Continue(());
                }
                self.fail_all_connections(&err);
                return ControlFlow::Continue(());
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
                self.fail_all_connections(&e);
            }
            return ControlFlow::Continue(());
        }
        // All work has been done. Check, if the pipe hasn't been closed.
        match self.rx.recv() {
            Ok(request) => {
                trace!("resumed work");
                self.enqueue_incoming_connection(request);
            }
            // Pipe has been closed. Shut down worker.
            Err(_) => {
                info!("shutting down");
                return ControlFlow::Break(());
            }
        }
        ControlFlow::Continue(())
    }

    fn fail_all_connections(&mut self, error: &curl::MultiError) {
        error!("stopping all HTTP connections because got error: {error}");
        for (_, (_, sender)) in self.connections.drain() {
            let _ = sender.send(CompletedRequest {
                response: Err(error.clone()).context("failed to execute curl mutli"),
            });
        }
    }
}
