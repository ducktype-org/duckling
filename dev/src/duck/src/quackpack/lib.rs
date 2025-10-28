use anyhow::Result;
pub type QuackResult<T> = Result<T>;
mod error;
pub mod paths;
pub mod schemas;
mod str_id;
pub mod toml_config;
pub use error::InternalError;
pub use str_id::*;
