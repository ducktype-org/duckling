// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};
use crate::quackpack::subcommands::sync::{SyncOptions, sync};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `sync` subcommand.
pub fn get_parser() -> Command {
    subcommand("sync")
        .about("Synchronize the current venv")
        .arg(flag("frozen", "Don't update the freezefile"))
        .arg(
            flag(
                "overwrite",
                "Overwrite any existing virtual environments with the same name",
            )
            .conflicts_with("global"),
        )
        .arg(
            flag("global", "Synchronize the global virtual environment")
                .conflicts_with("overwrite"),
        )
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
}

/// Logic for executing the `sync` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let options = SyncOptions {
        global: matches.get_flag("global"),
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
    };
    sync(ctx, options)
}
