use std::ffi::{OsStr, OsString};
use std::path::Path;

use clap::ArgMatches;
use tracing::debug;

use crate::duck::driver::cli_ext::jobs_from_matches;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::profiles::{DEFAULT_SCRIPT_PROFILE_NAME, Profile};
use crate::quackpack::core::storage::{StorageSyncOptions, SyncOutput, sync};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail_internal};

pub struct RunScriptOptions<'duck, 'matches> {
    /// Current [`DuckContext`].
    pub ctx: &'duck DuckContext,
    /// Path the script to run.
    pub path: &'duck Path,
    /// Force the script to be run in the global venv.
    pub global: bool,
    /// Profile to run the script in.
    pub profile: StrId,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
    /// Arguments to the script.
    pub args: Vec<&'matches OsStr>,
    /// Number of threads to use.
    pub jobs: usize,
}

impl<'duck, 'matches> RunScriptOptions<'duck, 'matches> {
    /// Create [`RunScriptOptions`] from a given [`Path`] and [`ArgMatches`].
    pub fn from_path_and_matches(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        matches: &'matches ArgMatches,
    ) -> QuackResult<Self> {
        let profile = matches
            .get_one::<String>("profile")
            .map(String::as_str)
            .unwrap_or(DEFAULT_SCRIPT_PROFILE_NAME)
            .into();
        let args: Vec<&OsStr> = matches
            .get_many::<OsString>("args")
            .unwrap_or_default()
            .map(OsString::as_os_str)
            .collect();
        debug!(?args);
        Ok(Self {
            ctx,
            path,
            global: matches.get_flag("global"),
            profile,
            overwrite: matches.get_flag("overwrite"),
            frozen: matches.get_flag("frozen"),
            strict_errors: matches.get_flag("external-errors"),
            args,
            jobs: jobs_from_matches(matches),
        })
    }

    /// Create [`RunScriptOptions`] from a given [`Path`] and a list of arguments to pass to the script.
    /// Supplies default values for other fields.
    pub fn from_path_and_args_with_defaults(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        args: Vec<&'matches OsStr>,
    ) -> QuackResult<Self> {
        let profile = DEFAULT_SCRIPT_PROFILE_NAME.into();
        debug!(?args);
        Ok(Self {
            ctx,
            path,
            global: false,
            profile,
            overwrite: false,
            frozen: false,
            strict_errors: false,
            args,
            jobs: 1,
        })
    }
}

/// Run script given options.
#[expect(unreachable_code, unused_variables)]
pub fn run_script<'duck, 'matches>(
    rs_options: RunScriptOptions<'duck, 'matches>,
) -> QuackResult<()> {
    // @TODO: #2900 Unmock this.
    qp_bail_internal!("@TODO: #2900 Pass scripts through `Unit`s");
    let RunScriptOptions {
        ctx,
        path,
        global,
        profile,
        overwrite,
        frozen,
        strict_errors,
        args,
        jobs: _,
    } = rs_options;
    let script_name = path.file_name().with_context_internal(|| {
        format!("path `{path:?}` does not have a filename, but we checked that earlier?")
    })?;
    let folder_path = path.parent().with_context_internal(|| {
        format!("path `{path:?}` does not have a parent folder, but we checked that earlier?")
    })?;

    let package = PackageLoader::load_script(ctx, path, folder_path, global)?;
    let root_identity = package.package().as_a_local_identity()?;
    let SyncOutput {
        new_freeze: _freeze,
        loaded_packages: _pkgs,
        sync_lock: lock,
        new_venv: _,
        storage,
    } = sync(
        &package,
        StorageSyncOptions {
            overwrite,
            frozen,
            strict_errors,
        },
    )?;
    let compile_lock = lock.into_compile_lock();
    let profile = Profile::construct_profile(profile, package.package().manifest().profiles())?;
}
