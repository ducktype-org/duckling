//! Our implementations of the [`Handler`] trait.

use std::io::{Cursor, Read};
use std::str::FromStr;

use curl::easy::Handler;
use http::{HeaderName, HeaderValue};

use super::{Response, try_parse_header_value};

#[derive(Clone, Debug, Default)]
/// A basic collector which saves the entire HTTP response as a vector of `u8`.
pub struct Collector {
    response: Response,
    // Cursor so we advance the position.
    request_body: Cursor<Vec<u8>>,
}

impl Collector {
    /// Create a new [`ResponseCollector`].
    pub fn new() -> Self {
        Self::default()
    }

    pub fn response(&self) -> &Response {
        &self.response
    }

    pub fn request_body(&self) -> &Cursor<Vec<u8>> {
        &self.request_body
    }

    pub fn request_body_mut(&mut self) -> &mut Cursor<Vec<u8>> {
        &mut self.request_body
    }

    pub fn set_request_body(&mut self, request_body: Vec<u8>) {
        self.request_body = Cursor::new(request_body);
    }
}

impl Handler for Collector {
    fn write(&mut self, data: &[u8]) -> Result<usize, curl::easy::WriteError> {
        self.response.body_mut().extend_from_slice(data);
        Ok(data.len())
    }

    fn read(&mut self, data: &mut [u8]) -> Result<usize, curl::easy::ReadError> {
        let len = self
            .request_body
            .read(data)
            .expect("read on Vec<u8> has returned an error?!");
        Ok(len)
    }

    fn header(&mut self, data: &[u8]) -> bool {
        if let Some((header, value)) = try_parse_header_value(data)
            && let Ok(header) = HeaderName::from_str(header)
            && let Ok(value) = HeaderValue::from_str(value)
        {
            self.response.headers_mut().insert(header, value);
        }
        true
    }
}
