// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::io::{self, Write};

use clap::{Arg, ArgAction, ArgMatches, Command, value_parser};
use clap_complete::{Generator, Shell, generate};

use crate::duck::driver::cli;
use crate::duck::driver::cli_ext::subcommand;
use crate::{DuckContext, QuackResult, qp_bail_internal};

/// Creates parser for the `generate` subcommand.
pub fn get_parser() -> Command {
    subcommand("generate")
        .about("Generate shell completions")
        .arg(
            Arg::new("generator")
                .help("Choose the target shell")
                .action(ArgAction::Set)
                .required(true)
                .value_parser(value_parser!(Shell)),
        )
}

/// Logic for executing the `generate` subcommand.
pub fn execute(_ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let Some(generator) = matches.get_one::<Shell>("generator").cloned() else {
        qp_bail_internal!("no generator; this should be guarded by the parser")
    };
    print_completions(generator, cli());
    Ok(())
}

/// Print shell completion file to the stdout.
///
/// This is the only exception from printing to stderr, and it also ignores `--quiet`.
fn print_completions<G: Generator>(generator: G, mut cli: Command) {
    let name = cli.get_name().to_string();
    generate(generator, &mut cli, name, io::stdout().lock().by_ref());
}
