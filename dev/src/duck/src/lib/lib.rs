// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Duck-library is the CLI driver behind the `duck` binary: argument parsing and subcommand
//! dispatch. The work itself lives in two other crates of this workspace:
//! - [`quackpack`]: the package manager, with its [`subcommands`](quackpack::subcommands) and
//!   [`core`](quackpack::core) ([`solver`](quackpack::core::solver),
//!   [`fetcher`](quackpack::core::fetcher), [`storage`](quackpack::core::storage),
//!   [`compile`](quackpack::core::compile)),
//! - [`duck_core`]: what all duck tools share — [`DuckContext`], [`QuackError`], CLI helpers.
//!
//! Notable module: [`duck::driver`][], the handling of the CLI driver.
//!
//! More information can be found in [`readme.md`]s.
pub mod duck;

pub use duck::main;
pub use duck_core::util::str_id::*;
pub use duck_core::{
    DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    qp_internal,
};
