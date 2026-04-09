//! Our implementations of the [`Handler`] trait.

use std::{fs::File, io::Write, path::Path};

use curl::easy::Handler;
use serde::Deserialize;

use crate::{QuackResult, util::path_ops_ext::PathOpsExt};

#[derive(Clone, Debug, Default)]
/// A basic collector which saves the entire HTTP response as a vector of `u8`.
pub struct ResponseCollector {
    data: Vec<u8>,
}

impl ResponseCollector {
    /// Create a new [`ResponseCollector`].
    pub fn new() -> Self {
        Self::default()
    }

    /// Get the underlying bytes of an HTTP response.
    pub fn data(&self) -> &[u8] {
        &self.data
    }

    /// Helper for deserializing JSONs from [`data`](Self::data).
    pub fn deserialize_json<T: for<'de> Deserialize<'de>>(&self) -> QuackResult<T> {
        serde_json::from_slice(&self.data).map_err(Into::into)
    }
}

impl Handler for ResponseCollector {
    fn write(&mut self, data: &[u8]) -> Result<usize, curl::easy::WriteError> {
        self.data.extend_from_slice(data);
        Ok(data.len())
    }
}

#[derive(Debug)]
/// Collector which writes new bytes into a file.
pub struct FileWriter {
    file: File,
}

impl FileWriter {
    /// Create a new [`FileWriter`], which will write to the `path`.
    pub fn new(path: &Path) -> QuackResult<Self> {
        let file = path.touch()?;
        Ok(Self { file })
    }

    /// Flush the underlying file.
    pub fn flush(&mut self) -> QuackResult<()> {
        self.file.flush().map_err(Into::into)
    }
}

impl Handler for FileWriter {
    fn write(&mut self, data: &[u8]) -> Result<usize, curl::easy::WriteError> {
        self.file
            .write_all(data)
            .map_err(|_| curl::easy::WriteError::Pause)
            .map(|_| data.len())
    }
}
