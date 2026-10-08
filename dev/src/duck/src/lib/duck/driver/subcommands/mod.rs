// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{ArgMatches, Command};

use crate::{DuckContext, QuackResult};

mod add;
mod build;
mod clean_storage;
#[cfg(feature = "shell-completion")]
mod generate;
mod info;
mod init;
mod list;
mod remove;
mod repl;
mod run;
pub mod run_script;
mod sync;

/// Get parsers for all the builtin subcommands.
pub fn subcommands() -> Vec<Command> {
    vec![
        build::get_parser(),
        run::get_parser(),
        add::get_parser(),
        remove::get_parser(),
        #[cfg(feature = "shell-completion")]
        generate::get_parser(),
        init::get_parser(),
        run_script::get_parser(),
        sync::get_parser(),
        repl::get_parser(),
        list::get_parser(),
        info::get_parser(),
        clean_storage::get_parser(),
    ]
}

/// Function signature which subcommands execution logic follows.
pub type ExecFn = fn(&DuckContext, &ArgMatches) -> QuackResult<()>;

/// Get the [`ExecFn`] for the given subcommand name.
///
/// Returns [`None`], if `name` is not a valid duck builtin subcommand name.
pub fn exec_for(name: &str) -> Option<ExecFn> {
    let f = match name {
        "add" => add::execute,
        "build" => build::execute,
        "clean-storage" => clean_storage::execute,
        #[cfg(feature = "shell-completion")]
        "generate" => generate::execute,
        "info" => info::execute,
        "init" => init::execute,
        "list" => list::execute,
        "remove" => remove::execute,
        "repl" => repl::execute,
        "run" => run::execute,
        "run-script" => run_script::execute,
        "sync" => sync::execute,
        _ => return None,
    };
    Some(f)
}
