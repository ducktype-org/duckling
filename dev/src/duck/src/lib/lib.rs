pub mod duck;
pub mod quackpack;
pub mod util_common;

pub use quackpack::util::str_id::*;
pub use util_common::{duck_ctx::DuckCtx, error::InternalError};

pub type QuackResult<T> = anyhow::Result<T>;
