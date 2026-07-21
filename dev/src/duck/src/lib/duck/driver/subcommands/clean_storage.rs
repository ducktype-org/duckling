use std::path::PathBuf;

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{optional, subcommand};
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::subcommands::clean_storage::{
    CleanStorageMode, CleanStorageOptions, clean_storage,
};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `clean-storage` subcommand.
pub fn get_parser() -> Command {
    subcommand("clean-storage")
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

/// Logic for executing the `clean-storage` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let storage: PathBuf = matches
        .get_one::<PathBuf>("storage")
        .cloned()
        .unwrap_or(ctx.duck_home().storage().into_not_locked_path());
    let clean_mode = match matches.get_one::<String>("venv") {
        None => CleanStorageMode::CleanStorage,
        Some(venv) => CleanStorageMode::RemoveVenv(venv.to_venv_id()),
    };
    let options = CleanStorageOptions {
        ctx,
        mode: clean_mode,
        storage,
    };
    clean_storage(options)
}
