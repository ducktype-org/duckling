// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::subcommands::info::info;
use crate::{DuckContext, QuackResult};

/// Creates parser for the `info` subcommand.
pub fn get_parser() -> Command {
    subcommand("info").about("Get information about the package's venv")
}

/// Logic for executing the `info` subcommand.
pub fn execute(ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    let pkg_ctx = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?;
    info(pkg_ctx)
}
