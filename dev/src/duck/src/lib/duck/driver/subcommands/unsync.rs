// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use crate::{DuckContext, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{optional, subcommand};

/// Creates parser for the `unsync` subcommand.
pub fn get_parser() -> Command {
    subcommand("unsync")
        .about("Unsynchronize the chosen venv by removing its state from the storage")
        .arg(optional("venv-id", "Id of the venv to unsynchronize"))
}

/// Logic for executing the `unsync` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement unsync")
}
