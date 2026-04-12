//! Duck-library consists of two main modules: [`duck`], which is responsible for the binary side
//! of the duck-binary, and [`quackpack`], the main package manager logic.
//!
//! Notable modules are:
//! - [`duck::driver`][]: handling of the CLI driver,
//! - [`quackpack::subcommands`][]: main subcommands execution logic,
//! - [`quackpack::core`][]: core functionalities of the quackpack, which include:
//!     - [`solver`](quackpack::core::solver)[]: resolving the dependency graph(s),
//!     - [`fetcher`](quackpack::core::fetcher)[]: HTTP requests handlers,
//!     - [`storage`](quackpack::core::storage)[]: storages' and venvs' management,
//!     - [`compile`](quackpack::core::compile)[]: duckc integration(s).
//!
//! More information can be found in [`readme.md`]s.
pub mod duck;
pub mod quackpack;
pub mod util;

pub use duck::{main, util::duck_context::DuckContext};
pub use quackpack::util::{qp_ctx::QpCtx, str_id::*};
pub use util::error::{QuackError, QuackResultContext};

/// A common [`Result`] type used widely throughout the project.
pub type QuackResult<T> = Result<T, QuackError>;
