use std::path::PathBuf;

use crate::duck::util::indent::indent;
use crate::quackpack::core::storage::venv_id::VenvId;
use crate::quackpack::core::storage::{self, CleanOutput, delete_venv};
use crate::util::IsPlural;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Options for the clean-storage operation.
#[derive(Debug, Clone)]
pub struct CleanStorageOptions<'duck> {
    pub ctx: &'duck DuckContext,
    /// Venv to delete
    pub mode: CleanStorageMode,
    /// Path to the storage from which we want to list the venvs.
    pub storage_path: PathBuf,
}

/// Modes of the clean-storage operation.
#[derive(Debug, Clone, Copy)]
pub enum CleanStorageMode {
    /// Clean the whole storage from ephemeral venvs and unused packages.
    CleanStorage,
    /// Remove single venv from the storage.
    RemoveVenv(VenvId),
}

/// Logic for executing the `clean-storage` subcommand.
pub fn clean_storage(options: CleanStorageOptions) -> QuackResult<()> {
    let CleanStorageOptions {
        ctx,
        mode,
        storage_path,
    } = options;
    match mode {
        CleanStorageMode::RemoveVenv(venv_name) => delete_venv(ctx, &storage_path, venv_name),
        CleanStorageMode::CleanStorage => {
            let CleanOutput {
                removed_venvs,
                removed_packages,
                encountered_errors,
            } = storage::clean_storage(ctx, &storage_path)
                .context("when trying to clean the storage")?;
            ctx.console().print(format!(
                "Removed {} venv{}",
                removed_venvs.len(),
                removed_venvs.s_if_plural(),
            ))?;
            for venv in removed_venvs {
                ctx.console()
                    .print(indent(&format!("venv with id `{}`", venv), 2))?;
            }
            ctx.console().print(format!(
                "Removed {} package{}",
                removed_packages.len(),
                removed_packages.s_if_plural(),
            ))?;
            for pkg_path in removed_packages {
                ctx.console()
                    .print(indent(&format!("package at `{}`", pkg_path.display()), 2))?;
            }
            if encountered_errors.has_errors() {
                let count = encountered_errors.logged_errors();
                let plural = if count == 1 { "" } else { "s" };
                ctx.console()
                    .warning(format!("encountered {count} error{plural} during clean"))?;
            }
            for error in encountered_errors {
                ctx.error_console()
                    .error(format!("encountered error during clean: {error}"))?;
            }
            Ok(())
        }
    }
}
