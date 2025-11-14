use std::{error::Error, fmt};

pub struct InternalError {
    inner: anyhow::Error,
}

impl fmt::Display for InternalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.inner.fmt(f)
    }
}

impl fmt::Debug for InternalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.inner.fmt(f)
    }
}

impl From<anyhow::Error> for InternalError {
    fn from(value: anyhow::Error) -> Self {
        Self { inner: value }
    }
}

impl From<&'static str> for InternalError {
    fn from(value: &'static str) -> Self {
        Self {
            inner: anyhow::Error::msg(value),
        }
    }
}

impl From<String> for InternalError {
    fn from(value: String) -> Self {
        Self {
            inner: anyhow::Error::msg(value),
        }
    }
}

impl Error for InternalError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        self.inner.source()
    }
}
