use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::subcommands::clean_storage::{
    CleanStorageMode, CleanStorageOptions, clean_storage,
};
use crate::util::error::HintMessage;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Creates parser for the `clean-storage` subcommand.
pub fn get_parser() -> Command {
    subcommand("clean-storage")
        .about("Clean the from expired ephemeral venvs and unused packages")
        .arg(optional(
            "venv",
            "Remove only the venv with such name (regardless of whether it is ephemeral or not), do not clean packages",
        ))
        .arg(flag("global-storage", "Clean the global storage"))
}

/// Logic for executing the `clean-storage` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let storage_path = if matches.get_flag("global-storage") {
        ctx.default_storage_root().into_not_locked_path()
    } else {
        let pcx = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No).with_context(|| {
            HintMessage::new("to clean the global storage use `global-storage` flag")
        })?;
        pcx.storage_path().to_path_buf()
    };
    let clean_mode = match matches.get_one::<String>("venv") {
        None => CleanStorageMode::CleanStorage,
        Some(venv) => CleanStorageMode::RemoveVenv(venv.to_venv_id()),
    };
    let options = CleanStorageOptions {
        ctx,
        mode: clean_mode,
        storage_path,
    };
    clean_storage(options)
}
