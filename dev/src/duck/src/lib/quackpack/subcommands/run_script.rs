use std::{
    ffi::{OsStr, OsString},
    path::Path,
};

use clap::ArgMatches;

use crate::{
    DuckContext, QuackResult, QuackResultContext, qp_bail_internal,
    quackpack::core::{
        AllowGlobalPackage, PackageLoader,
        storage::{
            StorageSyncOptions, sync,
            venv_id::{ToVenvId, VenvId},
        },
    },
};

pub struct RunScriptOptions<'duck> {
    /// Current [`DuckCtx`].
    pub ctx: &'duck DuckContext,
    /// Name of the script to run.
    pub script_name: &'duck OsStr,
    /// Path to the folder where the script is located.
    pub folder_path: &'duck Path,
    /// Optional name of the venv to run the script in.
    pub venv_id: Option<VenvId>,
    /// Force the script to be run in the global venv.
    pub global: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
    /// Arguments to the script.
    pub args: Vec<OsString>,
}

impl<'duck> RunScriptOptions<'duck> {
    /// Create [`RunScriptOptions`] from a given [`Path`] and [`ArgMatches`].
    pub fn from_path_and_matches(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        matches: &ArgMatches,
    ) -> QuackResult<Self> {
        let script_name = path
            .file_name()
            .context_internal("we assured that the path points to a file")?;
        let folder_path = path
            .parent()
            .context_internal("we assured that the path points to a file")?;
        let venv_id = matches.get_one::<String>("venv").map(ToVenvId::to_venv_id);
        let args: Vec<OsString> = matches
            .get_many::<OsString>("args")
            .map(|values| values.cloned().collect())
            .unwrap_or_default();
        Ok(Self {
            ctx,
            script_name,
            folder_path,
            venv_id,
            global: matches.get_flag("global"),
            overwrite: matches.get_flag("overwrite"),
            frozen: matches.get_flag("frozen"),
            strict_errors: matches.get_flag("external-errors"),
            args,
        })
    }

    /// Create [`RunScriptOptions`] from a given [`Path`] and a list of arguments to pass to the script.
    /// Supplies default values for other fields.
    pub fn from_path_and_args_with_defaults(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        args: Vec<OsString>,
    ) -> QuackResult<Self> {
        let script_name = path
            .file_name()
            .context_internal("we assured that the path points to a file")?;
        let folder_path = path
            .parent()
            .context_internal("we assured that the path points to a file")?;
        Ok(Self {
            ctx,
            script_name,
            folder_path,
            venv_id: None,
            global: false,
            overwrite: false,
            frozen: false,
            strict_errors: false,
            args,
        })
    }
}

/// Run script given options.
pub fn run_script<'duck>(rs_options: RunScriptOptions<'duck>) -> QuackResult<()> {
    let package = match rs_options.venv_id {
        Some(venv_id) => {
            debug_assert!(!rs_options.global, "should be guarded by the parser");
            PackageLoader::find_venv_by_name(rs_options.ctx, venv_id)?
        }
        None => {
            if rs_options.global {
                PackageLoader::global_package(rs_options.ctx)?
            } else {
                PackageLoader::find_from_directory(
                    rs_options.folder_path,
                    rs_options.ctx,
                    AllowGlobalPackage::Yes,
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
