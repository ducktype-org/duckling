pub mod duck;
pub mod quackpack;
pub use duck::duck_ctx::DuckCtx;
pub use duck::error::InternalError;
pub use duck::terminal;
pub use quackpack::str_id::*;
pub type QuackResult<T> = anyhow::Result<T>;
