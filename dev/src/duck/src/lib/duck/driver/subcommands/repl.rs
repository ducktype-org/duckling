// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::subcommands::repl::{ReplOptions, repl};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `repl` subcommand.
pub fn get_parser() -> Command {
    subcommand("repl")
        .about("Start a REPL session")
        .long_about(
            "Start a REPL session.\n\n\
             Inside a package, cells can import the package and its dependencies. \
             Outside of one (or with `--global`), they can import the global packages \
             (`duck add -g`).",
        )
        .arg(flag(
            "global",
            "Use the global packages even when inside a package",
        ))
        .arg(flag("frozen", "Don't update the freezefile"))
        .arg(flag(
            "overwrite",
            "Overwrite any existing virtual environments with the same name",
        ))
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
        .arg(
            Arg::new("duckc-args")
                .help("Extra arguments passed to `duckc repl`, after `--`")
                .num_args(0..)
                .last(true)
                .action(ArgAction::Append),
        )
}

/// Logic for executing the `repl` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let pcx = if matches.get_flag("global") {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::Yes)?
    };
    let duckc_args = matches
        .get_many::<String>("duckc-args")
        .map(|args| args.cloned().collect())
        .unwrap_or_default();
    repl(ReplOptions {
        pcx,
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
        duckc_args,
    })
    .map(|_| ())
}

/// Start a REPL session with default options, as `duck` without a subcommand does.
pub fn execute_default(ctx: &DuckContext) -> QuackResult<()> {
    execute(ctx, &get_parser().get_matches_from(["repl"]))
}
