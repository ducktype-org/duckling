//! General HTTP client, backed by [`curl`].
use std::path::Path;

use curl::easy::{Easy2, Handler, List};
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http::handlers::{FileWriter, ResponseCollector};
use crate::{DuckContext, qp_bail};
use crate::{QuackResult, QuackResultContext};

mod defaults;
mod handlers;

#[derive(Debug, Clone)]
/// Our implementation of a curl-backed HTTP client.
pub struct HttpClient<'duck> {
    _ctx: &'duck DuckContext,
}

impl<'duck> HttpClient<'duck> {
    /// Construct a new [`HttpClient`].
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self { _ctx: ctx }
    }

    /// Create a common [`Easy2`] handler.
    fn create_easy<H: Handler>(handler: H) -> QuackResult<Easy2<H>> {
        let mut easy = Easy2::new(handler);
        easy.useragent(defaults::DUCK_USER_AGENT)?;
        // Accept all encodings.
        easy.accept_encoding("")?;
        easy.max_redirections(defaults::MAX_REDIRECTS as u32)?;
        easy.connect_timeout(defaults::CONNECT_TIMEOUT)?;
        easy.timeout(defaults::REQUEST_TIMEOUT)?;
        easy.follow_location(true)?;
        let mut headers = List::new();
        headers.append(defaults::EXPECT_HEADER_WITH_VALUE)?;
        headers.append(defaults::PRAGMA_HEADER_WITH_VALUE)?;
        easy.http_headers(headers)?;
        Ok(easy)
    }

    /// Perform a general HTTP GET request.
    #[tracing::instrument(skip(self))]
    pub fn get(&self, url: &Url) -> QuackResult<ResponseCollector> {
        let mut easy = Self::create_easy(ResponseCollector::default())?;
        easy.get(true)?;
        easy.url(url.as_str())?;
        easy.perform()
            .context("failed to perform an http request")?;
        let code = easy.response_code()?;
        Self::bail_for_error_code(code, url)?;
        Ok(easy.get_ref().clone())
    }

    /// Perform a general HTTP GET request, and save response to a file at `path`.
    #[tracing::instrument(skip(self))]
    pub fn get_to_file(&self, url: &Url, path: &Path) -> QuackResult<()> {
        let mut easy = Self::create_easy(FileWriter::new(path)?)?;
        easy.get(true)?;
        easy.url(url.as_str())?;
        easy.perform()
            .context("failed to perform an http request")?;
        let code = easy.response_code()?;
        Self::bail_for_error_code(code, url)?;
        easy.get_mut()
            .flush()
            .with_context(|| format!("failed to flush `{}`", path.display()))
    }

    /// Helper for checking HTTP status codes.
    fn bail_for_error_code(code: u32, path: &Url) -> QuackResult<()> {
        debug!("got HTTP code {code}");
        let description = match code {
            100..200 => "informational",
            200..300 => return Ok(()),
            300..400 => "redirect",
            400..500 => "client error",
            500..600 => "server error",
            _ => "unknown error",
        };
        qp_bail!("HTTP status {description} ({code}) for url `{path}`")
    }
}
