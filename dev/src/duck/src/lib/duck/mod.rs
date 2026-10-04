//! Main duck driver implementation.
//! Mainly, a home of the [`driver`] module, and [`DuckContext`](util::duck_context::DuckContext) struct.
pub mod driver;
mod main;
pub mod util;
pub mod version;

pub use main::{main, setup_logger};
