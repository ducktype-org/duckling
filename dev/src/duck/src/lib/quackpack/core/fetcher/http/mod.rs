//! General HTTP client, backed by [`curl`].

use curl::easy::{Easy2, Handler};

use super::util::http::handlers::Collector;
use super::util::http::{Request, Response, check_http_status_code, configure_easy2};
use crate::{DuckContext, QuackResult, QuackResultContext};

#[derive(Debug, Clone)]
/// Our implementation of a curl-backed HTTP client.
pub struct HttpClient<'duck> {
    ctx: &'duck DuckContext,
}

impl<'duck> HttpClient<'duck> {
    /// Construct a new [`HttpClient`].
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self { ctx }
    }

    /// Get the underlying [`DuckContext`].
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }

    /// Create a common [`Easy2`] handler.
    fn create_easy<H: Handler>(&self, handler: H, request: &Request) -> QuackResult<Easy2<H>> {
        let mut easy = Easy2::new(handler);
        configure_easy2(&mut easy, self.ctx, request)?;
        Ok(easy)
    }

    /// Perform a generic HTTP request.
    #[tracing::instrument(skip_all)]
    pub fn request(&self, request: Request) -> QuackResult<Response> {
        let mut easy = self.create_easy(Collector::default(), &request)?;
        let (parts, body) = request.into_parts();
        if parts.method == http::Method::POST {
            easy.get_mut().set_request_body(body);
        }
        easy.perform()
            .context("failed to perform an HTTP request")?;
        let code = easy.response_code()?;
        check_http_status_code(code, &parts.uri)?;
        Ok(easy.get_ref().response().clone())
    }
}
