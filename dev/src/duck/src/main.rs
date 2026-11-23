// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The CLI driver behind the `duck` binary: argument parsing and subcommand dispatch. The work
//! itself lives in two other crates of this workspace:
//! - [`quackpack`]: the package manager, with its [`subcommands`](quackpack::subcommands) and
//!   [`core`](quackpack::core) ([`solver`](quackpack::core::solver),
//!   [`fetcher`](quackpack::core::fetcher), [`storage`](quackpack::core::storage),
//!   [`compile`](quackpack::core::compile)),
//! - [`duck_core`]: what all duck tools share — [`DuckContext`], [`QuackError`], CLI helpers. It is
//!   re-exported here, so `crate::` paths reach it too.
//!
//! Notable module: [`driver`], the handling of the CLI driver.
//!
//! More information can be found in [`readme.md`]s.
pub mod driver;
pub mod version;

pub use duck_core::*;
use tracing::trace;

/// The entry point of the duck binary: the common duck tool entry point, driving [`duck::run`].
fn main() {
    trace!("linked against `{:#?}`", curl::Version::get());
    duck_core::pipeline::main(run)
}

/// Run `duck` with the given context: parse the command line and execute the subcommand.
pub fn run(ctx: &mut DuckContext) -> QuackResult<()> {
    driver::run::run(ctx)
}
