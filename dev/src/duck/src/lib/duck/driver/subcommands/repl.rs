// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;
use crate::quackpack::core::compile::duckc::Duckc;
use crate::{DuckContext, QuackResult};

/// Creates parser for the `repl` subcommand.
pub fn get_parser() -> Command {
    subcommand("repl").about("Start a REPL session")
}

/// Logic for executing the `repl` subcommand.
pub fn execute(ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    Duckc::start_repl_with(ctx).map(|_| ())
}
