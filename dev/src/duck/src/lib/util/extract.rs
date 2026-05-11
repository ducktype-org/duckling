//! Extract a value.

use std::sync::LockResult;

use tracing::error;

pub trait Extract<T> {
    /// Extract a value.
    fn extract(self) -> T;
}

impl<T> Extract<T> for LockResult<T> {
    fn extract(self) -> T {
        match self {
            Self::Ok(value) => value,
            Self::Err(poison) => {
                error!("{poison} ({poison:?}");
                poison.into_inner()
            }
        }
    }
}
