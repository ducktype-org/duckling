//! Main duck driver implementation.
//! Mainly, a home of the [`driver`] module, and [`DuckCtx`](util::duck_ctx::DuckCtx) struct.
pub mod driver;
mod main;
pub mod util;

pub use main::{main, setup_logger};
