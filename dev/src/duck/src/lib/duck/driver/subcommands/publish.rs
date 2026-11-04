// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use crate::{DuckContext, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `publish` subcommand.
pub fn get_parser() -> Command {
    subcommand("publish").about("Publish the current package to the registry")
}

/// Logic for executing the `publish` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement publish")
}
