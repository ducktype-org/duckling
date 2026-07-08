use curl::easy::{self, Easy2};
use tracing::debug;
use url::Url;

use crate::{DuckContext, QuackResult, qp_bail};

pub mod defaults;
pub mod handlers;

/// Configure an [`Easy2`] handler.
pub fn configure_easy2<H>(handler: &mut Easy2<H>, ctx: &DuckContext) -> Result<(), curl::Error> {
    let _ = ctx;
    handler.useragent(defaults::DUCK_USER_AGENT)?;
    // Accept all encodings.
    handler.accept_encoding("")?;
    handler.max_redirections(defaults::MAX_REDIRECTS as u32)?;
    handler.connect_timeout(defaults::CONNECT_TIMEOUT)?;
    handler.timeout(defaults::REQUEST_TIMEOUT)?;
    handler.follow_location(true)?;
    let mut headers = easy::List::new();
    headers.append(defaults::EXPECT_HEADER_WITH_VALUE)?;
    headers.append(defaults::PRAGMA_HEADER_WITH_VALUE)?;
    handler.http_headers(headers)?;
    Ok(())
}

/// Helper for checking HTTP status codes.
pub fn check_http_status_code(code: u32, path: &Url) -> QuackResult<()> {
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
