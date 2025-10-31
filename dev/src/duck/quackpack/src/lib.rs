use anyhow::Result;
pub type QuackResult<T> = Result<T>;
pub mod paths;
mod str_id;
pub mod toml_config;
pub use str_id::*;
