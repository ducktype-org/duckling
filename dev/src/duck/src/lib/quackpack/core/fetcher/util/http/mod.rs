use curl::easy::{self, Easy2};
use http::header;
use tracing::{debug, error};

use self::handlers::Collector;
use crate::{DuckContext, QuackResult, qp_bail};

pub mod defaults;
pub mod handlers;
pub mod traits_extensions;

pub type Request = http::Request<Vec<u8>>;
pub type Response = http::Response<Vec<u8>>;

const HTTP_DELIMITER: &str = "\r\n";

/// Configure an [`Easy2`] handler.
pub fn configure_easy2<H>(
    handler: &mut Easy2<H>,
    ctx: &DuckContext,
    request: &Request,
) -> Result<(), curl::Error> {
    let _ = ctx;
    let get_header_with_fallback = |key, fallback| {
        request
            .headers()
            .get(key)
            .and_then(|value| value.to_str().ok())
            .unwrap_or(fallback)
    };
    handler.useragent(get_header_with_fallback(
        header::USER_AGENT,
        defaults::DUCK_USER_AGENT,
    ))?;
    // Accept all encodings.
    handler.accept_encoding(get_header_with_fallback(header::ACCEPT_ENCODING, ""))?;
    if let Some(value) = request.headers().get(header::MAX_FORWARDS)
        && let Ok(value) = value.to_str()
        && let Ok(num) = value.parse()
    {
        handler.max_redirections(num)?;
    } else {
        handler.max_redirections(defaults::MAX_REDIRECTS as u32)?;
    }
    handler.connect_timeout(defaults::CONNECT_TIMEOUT)?;
    handler.timeout(defaults::REQUEST_TIMEOUT)?;
    handler.follow_location(true)?;
    let headers = http_headers_to_curl_list(request.headers())?;
    handler.http_headers(headers)?;
    handler.url(&request.uri().to_string())?;
    set_http_method_on_curl(handler, request.method())?;
    set_http_version_on_curl(handler, request.version())?;
    handler.pipewait(true)?;
    Ok(())
}

/// Convert an [`http::header::HeaderMap`] into a [`curl::easy::List`] headers.
fn http_headers_to_curl_list(headers: &header::HeaderMap) -> Result<easy::List, curl::Error> {
    let mut list = easy::List::new();
    for (header, value) in headers.iter() {
        let value = match value.to_str() {
            Ok(value) => value,
            Err(err) => {
                error!(
                    "while converting the value of the header `{header}` ({}) to the str: {err}",
                    String::from_utf8_lossy(value.as_bytes())
                );
                continue;
            }
        };
        if !value.trim().is_empty() {
            list.append(&format!("{header}: {value}"))?;
        } else {
            list.append(&format!("{header};"))?;
        }
    }

    Ok(list)
}

/// Set an HTTP method on the given handler.
///
/// Most methods map to [`custom_request`](Easy2::custom_request).
fn set_http_method_on_curl<H>(
    handler: &mut Easy2<H>,
    method: &http::Method,
) -> Result<(), curl::Error> {
    match *method {
        http::Method::GET => handler.get(true)?,
        http::Method::POST => handler.post(true)?,
        http::Method::PUT => handler.put(true)?,
        _ => handler.custom_request(method.as_str())?,
    };
    Ok(())
}

/// Set an HTTP version on the given handler.
fn set_http_version_on_curl<H>(
    handler: &mut Easy2<H>,
    version: http::Version,
) -> Result<(), curl::Error> {
    match version {
        http::Version::HTTP_09 => handler.http_09_allowed(true)?,
        http::Version::HTTP_10 => handler.http_version(curl::easy::HttpVersion::V10)?,
        http::Version::HTTP_11 => handler.http_version(curl::easy::HttpVersion::V11)?,
        http::Version::HTTP_2 => handler.http_version(curl::easy::HttpVersion::V2)?,
        http::Version::HTTP_3 => handler.http_version(curl::easy::HttpVersion::V3)?,
        _ => error!("unknown HTTP version: `{version:?}`, ignoring..."),
    };
    Ok(())
}

/// Helper for checking HTTP status codes.
pub fn check_http_status_code(response: &Response, path: &http::Uri) -> QuackResult<()> {
    let code = response.status().as_u16();
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

/// Try to parse an HTTP header-value pair.
/// It's mostly used as a callback.
///
/// Format is `HEADER: VALUE` or `HEADER: VALUE\r\n`.
pub fn try_parse_header_value(data: &[u8]) -> Option<(&str, &str)> {
    let data = data
        .strip_suffix(HTTP_DELIMITER.as_bytes())
        .unwrap_or_else(|| {
            error!(
                "HTTP Header `{}` doesn't end in `\\r\\n",
                String::from_utf8_lossy(data)
            );
            data
        });
    if data.is_empty() {
        return None;
    }
    let data = str::from_utf8(data).ok()?;
    let (header, value) = data.split_once(':')?;
    let value = value.trim();
    Some((header, value))
}

/// Extract a [`Response`] from a [`Easy2<Collector>`] handle.
pub fn response_from_handler(handler: Easy2<Collector>) -> Response {
    let mut response = handler.get_ref().response().clone();
    if let Ok(status) = handler.response_code()
        && status != 0
        && let Ok(status) = http::StatusCode::from_u16(status as u16)
    {
        *response.status_mut() = status;
    }
    response
}
