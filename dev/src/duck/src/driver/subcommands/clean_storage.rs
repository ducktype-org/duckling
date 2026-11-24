// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::{ArgMatches, Command};
use quackpack::core::storage::venv_id::ToVenvId;
use quackpack::core::{AllowGlobalPackage, PackageLoader};
use quackpack::subcommands::clean_storage::{CleanStorageMode, CleanStorageOptions, clean_storage};

use crate::cli::{flag, optional, subcommand};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `clean-storage` subcommand.
pub fn get_parser() -> Command {
    subcommand("clean-storage")
        .about("Clean the expired ephemeral venvs and unused packages")
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
        let pcx =
            PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No).map_err(|mut err| {
                err.add_hint("to clean the global storage use `global-storage` flag");
                err
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
