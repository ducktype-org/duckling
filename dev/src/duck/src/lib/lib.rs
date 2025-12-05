pub mod duck;
pub mod quackpack;
pub mod util_common;

pub use duck::{main, util::duck_ctx::DuckCtx};
pub use quackpack::util::{qp_ctx::QpCtx, str_id::*};
pub use util_common::error::{QuackError, QuackResultContext};


pub type QuackResult<T> = Result<T, QuackError>;