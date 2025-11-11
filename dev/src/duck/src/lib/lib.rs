pub mod duck_cfg;
pub mod duck_ctx;
pub mod error;
pub mod terminal;
pub mod toml_config;

pub mod quackpack;

pub use duck_ctx::DuckCtx;
pub use error::InternalError;
pub use quackpack::str_id::*;

pub type QuackResult<T> = anyhow::Result<T>;
