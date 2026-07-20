use std::path::PathBuf;

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{optional, subcommand};
use crate::quackpack::subcommands::clean::{CleanMode, CleanOptions, clean};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `clean` subcommand.
pub fn get_parser() -> Command {
    subcommand("clean")
        .about("Clean the storage")
        .long_about("Clean the storage from expired ephemeral venvs and unused packages.")
        .arg(optional(
            "venv",
            "Remove only the venv with such name (regardless of whether it is ephemeral or not), do not clean packages",
        ))
        .arg(optional(
            "storage",
            "Path to the storage from which to list venvs",
        ))
}

/// Logic for executing the `clean` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let storage: PathBuf = matches
        .get_one::<PathBuf>("storage")
        .cloned()
        .unwrap_or(ctx.duck_home().storage().into_not_locked_path());
    let clean_mode = match matches.get_one::<String>("venv") {
        None => CleanMode::CleanStorage,
        Some(venv) => CleanMode::RemoveVenv(venv.into()),
    };
    let options = CleanOptions {
        ctx,
        mode: clean_mode,
        storage,
    };
    clean(options)
}
