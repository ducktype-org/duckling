use anyhow::Error;
use thiserror::Error;

#[derive(Error, Debug)]
#[error("{source}")]
pub struct InternalError {
    #[source]
    #[from]
    source: Error,
}
