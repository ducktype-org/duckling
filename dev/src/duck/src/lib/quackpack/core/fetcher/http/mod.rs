//! General HTTP client, backed by [`curl`].
use std::path::Path;

use curl::easy::{Easy2, Handler};
use url::Url;

use crate::quackpack::util::http::handlers::{FileWriter, ResponseCollector};
use crate::quackpack::util::http::{check_http_status_code, configure_easy2};
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

    /// Create a common [`Easy2`] handler.
    fn create_easy<H: Handler>(&self, handler: H) -> QuackResult<Easy2<H>> {
        let mut easy = Easy2::new(handler);
        configure_easy2(&mut easy, self.ctx)?;
        Ok(easy)
    }

    /// Perform a general HTTP GET request.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub fn get(&self, url: &Url) -> QuackResult<ResponseCollector> {
        let mut easy = self.create_easy(ResponseCollector::default())?;
        easy.get(true)?;
        easy.url(url.as_str())?;
        easy.perform()
            .context("failed to perform an http request")?;
        let code = easy.response_code()?;
        check_http_status_code(code, url)?;
        Ok(easy.get_ref().clone())
    }

    /// Perform a general HTTP GET request, and save response to a file at `path`.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub fn get_to_file(&self, url: &Url, path: &Path) -> QuackResult<()> {
        let mut easy = self.create_easy(FileWriter::new(path)?)?;
        easy.get(true)?;
        easy.url(url.as_str())?;
        easy.perform()
            .context("failed to perform an http request")?;
        let code = easy.response_code()?;
        check_http_status_code(code, url)?;
        easy.get_mut().flush()
    }
}
