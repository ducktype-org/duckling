use std::{ffi::OsStr, path::Path};

use crate::{
    DuckCtx, QuackResult, StrId, qp_bail_internal,
    quackpack::core::{
        AllowGlobalPackage, PackageLoader,
        storage::{StorageSyncOptions, sync},
    },
};

pub struct RunScriptOptions<'duck> {
    /// Current [`DuckCtx`].
    pub ctx: &'duck DuckCtx,
    /// Name of the script to run.
    pub script_name: &'duck OsStr,
    /// Path to the folder where the script is located.
    pub folder_path: &'duck Path,
    /// Optional name of the venv to run the script in.
    pub venv_id: Option<StrId>,
    /// Force the script to be run in the global venv.
    pub global: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
}

/// Run script given options.
pub fn run_script<'duck>(rs_options: RunScriptOptions<'duck>) -> QuackResult<()> {
    let package = match rs_options.venv_id {
        Some(venv_id) => {
            debug_assert!(!rs_options.global);
            PackageLoader::find_venv_by_name(rs_options.ctx, venv_id)?
        }
        None => {
            if rs_options.global {
                PackageLoader::global_package(rs_options.ctx)?
            } else {
                PackageLoader::find_active_or_from_directory(
                    rs_options.folder_path,
                    rs_options.ctx,
                    AllowGlobalPackage::Yes,
                    true,
                )?
            }
        }
    };
    let (_lock, _venv, _storage) = sync(
        &package,
        StorageSyncOptions {
            overwrite: rs_options.overwrite,
            frozen: rs_options.frozen,
            strict_errors: rs_options.strict_errors,
        },
    )?;
    qp_bail_internal!("Implement compiling and running scripts")
}
