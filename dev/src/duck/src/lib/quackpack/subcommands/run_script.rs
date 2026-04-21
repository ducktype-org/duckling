use std::ffi::{OsStr, OsString};
use std::path::Path;

use clap::ArgMatches;
use tracing::debug;

use crate::quackpack::core::compile::duckc::{ArtifactsDir, CompilationType};
use crate::quackpack::core::compile::profiles::{DEFAULT_SCRIPT_PROFILE_NAME, Profile};
use crate::quackpack::core::compile::{self, BuildContext};
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::quackpack::core::storage::{StorageSyncOptions, sync};
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader, run};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail_internal};

pub struct RunScriptOptions<'duck> {
    /// Current [`DuckContext`].
    pub ctx: &'duck DuckContext,
    /// Path the script to run.
    pub path: &'duck Path,
    /// Optional name of the venv to run the script in.
    pub venv_id: Option<VenvId>,
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
    pub args: Vec<OsString>,
}

impl<'duck> RunScriptOptions<'duck> {
    /// Create [`RunScriptOptions`] from a given [`Path`] and [`ArgMatches`].
    pub fn from_path_and_matches(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        matches: &ArgMatches,
    ) -> QuackResult<Self> {
        let venv_id = matches.get_one::<String>("venv").map(ToVenvId::to_venv_id);
        let profile = matches
            .get_one::<String>("profile")
            .map(String::as_str)
            .unwrap_or(DEFAULT_SCRIPT_PROFILE_NAME)
            .into();
        let args: Vec<OsString> = matches
            .get_many::<OsString>("args")
            .map(|values| values.cloned().collect())
            .unwrap_or_default();
        debug!(?args);
        Ok(Self {
            ctx,
            path,
            venv_id,
            global: matches.get_flag("global"),
            profile,
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
        let profile = DEFAULT_SCRIPT_PROFILE_NAME.into();
        debug!(?args);
        Ok(Self {
            ctx,
            path,
            venv_id: None,
            global: false,
            profile,
            overwrite: false,
            frozen: false,
            strict_errors: false,
            args,
        })
    }
}

/// Run script given options.
pub fn run_script<'duck>(rs_options: RunScriptOptions<'duck>) -> QuackResult<()> {
    let RunScriptOptions {
        ctx,
        path,
        venv_id,
        global,
        profile,
        overwrite,
        frozen,
        strict_errors,
        args,
    } = rs_options;
    let script_name = path
        .file_name()
        .context_internal("we assured that the path points to a file")?;
    let folder_path = path
        .parent()
        .context_internal("we assured that the path points to a file")?;
    let package = match venv_id {
        Some(venv_id) => {
            debug_assert!(!global, "should be guarded by the parser");
            PackageLoader::find_venv_by_name(ctx, venv_id)?
        }
        None => {
            if global {
                PackageLoader::global_package(ctx)?
            } else {
                PackageLoader::find_from_directory(folder_path, ctx, AllowGlobalPackage::Yes)?
            }
        }
    };
    let (lock, venv, storage) = sync(
        &package,
        StorageSyncOptions {
            overwrite,
            frozen,
            strict_errors,
        },
    )?;
    let compile_lock = lock
        .to_compile_lock(&storage, package.to_venv_id())
        .context("failed to acquire a compile lock")?;
    let profile = Profile::construct_profile(profile, package.package().manifest().profiles())?;
    let bcx = BuildContext {
        pcx: &package,
        freeze: venv.into(),
        storage,
        used_features: vec![],
        profile,
        script_path: Some(folder_path.join(script_name)),
    };
    let artifacts_dir = compile::compile(bcx, CompilationType::StandaloneScript)?;
    drop(compile_lock);
    execute_script(artifacts_dir, script_name, profile.dvm_bytecode, args)
}

/// Run the created script binary.
fn execute_script(
    artifacts_dir: ArtifactsDir,
    script_name: &OsStr,
    dvm_backend: bool,
    args: Vec<OsString>,
) -> QuackResult<()> {
    let artifacts_dir = match artifacts_dir {
        ArtifactsDir::TempDir(dir) => dir,
        _ => qp_bail_internal!("script compilation did not produce a tempdir"),
    };
    if dvm_backend {
        panic!("@TODO: #2443 Implement run")
    } else {
        let exe_name = Path::new(script_name).with_extension("exe");
        let exe_path = artifacts_dir.path().join(exe_name);
        let exit_status = run::run_exe(&exe_path, args)?;
        drop(artifacts_dir);
        std::process::exit(exit_status.code().unwrap_or(0))
    }
}
