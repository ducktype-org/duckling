//! Extract a value.

use std::sync::LockResult;

use tracing::warn;

pub trait Extract<T> {
    /// Extract a value.
    fn extract(self) -> T;
}

impl<T> Extract<T> for LockResult<T> {
    fn extract(self) -> T {
        match self {
            Self::Ok(value) => value,
            Self::Err(poison) => {
                warn!(error = %poison, "poisoned");
                poison.into_inner()
            }
        }
    }
}
