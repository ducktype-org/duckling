//! Our implementations of the [`Handler`] trait.

use std::fs::File;
use std::io::Write;
use std::path::{Path, PathBuf};

use curl::easy::Handler;
use serde::Deserialize;
use tracing::error;

use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, QuackResultContext};

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
    pub fn deserialize_json<T: for<'de> Deserialize<'de>>(&self) -> Result<T, serde_json::Error> {
        serde_json::from_slice(&self.data)
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
    path: PathBuf,
}

impl FileWriter {
    /// Create a new [`FileWriter`], which will write to the `path`.
    pub fn new(path: &Path) -> QuackResult<Self> {
        let file = path.touch()?;
        Ok(Self {
            file,
            path: path.to_path_buf(),
        })
    }

    /// Flush the underlying file.
    pub fn flush(&mut self) -> QuackResult<()> {
        self.file
            .flush()
            .with_context(|| format!("failed to flush `{}`", self.path.display()))
    }
}

impl Handler for FileWriter {
    fn write(&mut self, data: &[u8]) -> Result<usize, curl::easy::WriteError> {
        match self.file.write_all(data) {
            Ok(_) => Ok(data.len()),
            Err(e) => {
                error!("failed to write data to `{}`: {e}", self.path.display());
                Err(curl::easy::WriteError::Pause)
            }
        }
    }
}
