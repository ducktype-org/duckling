use std::path::PathBuf;

use crate::duck::util::indent::indent;
use crate::quackpack::core::storage::{CleanOutput, clean_storage, delete_venv};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

/// Options for the clean operation.
#[derive(Debug, Clone)]
pub struct CleanOptions<'duck> {
    pub ctx: &'duck DuckContext,
    /// Venv to delete
    pub mode: CleanMode,
    /// Path to the storage from which we want to list the venvs.
    pub storage: PathBuf,
}

/// Modes of the clean operation.
#[derive(Debug, Clone, Copy)]
pub enum CleanMode {
    /// Clean the whole storage from ephemeral venvs and unused packages.
    CleanStorage,
    /// Remove single venv from the storage.
    RemoveVenv(StrId),
}

pub fn clean(options: CleanOptions) -> QuackResult<()> {
    let CleanOptions { ctx, mode, storage } = options;
    match mode {
        CleanMode::RemoveVenv(venv_name) => delete_venv(ctx, &storage, venv_name),
        CleanMode::CleanStorage => {
            let CleanOutput {
                removed_venvs,
                removed_packages,
            } = clean_storage(ctx, &storage).context("when trying to clean the storage")?;
            ctx.console().print(format!(
                "Removed {} venv{}",
                removed_venvs.len(),
                if removed_venvs.len() == 1 { "" } else { "s" }
            ))?;
            for venv in removed_venvs {
                ctx.console().print(indent(venv.as_ref(), 2))?;
            }
            ctx.console().print(format!(
                "Removed {} package{}",
                removed_packages.len(),
                if removed_packages.len() == 1 { "" } else { "s" }
            ))?;
            for pkg in removed_packages {
                ctx.console()
                    .print(indent(&format!("{}", pkg.display()), 2))?;
            }
            Ok(())
        }
    }
}
