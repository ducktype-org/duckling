// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use crate::{DuckContext, QuackResult, qp_bail};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `search` subcommand.
pub fn get_parser() -> Command {
    subcommand("search")
        .about("Search for a package in the registry")
        .arg(Arg::new("package").help("Package name"))
}

/// Logic for executing the `search` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement search")
}
