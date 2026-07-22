use std::ffi::OsString;
use std::path::Path;

use clap::ArgMatches;
use tracing::debug;

use crate::duck::driver::cli_ext::jobs_from_matches;
use crate::quackpack::core::compile::profiles::{DEFAULT_SCRIPT_PROFILE_NAME, Profile};
use crate::quackpack::core::storage::{StorageSyncOptions, sync};
use crate::quackpack::core::{AllowGlobalPackage, PackageContext, PackageLoader};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal};

pub struct RunScriptOptions<'duck> {
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
    pub args: Vec<OsString>,
    /// Number of threads to use.
    pub jobs: usize,
}

impl<'duck> RunScriptOptions<'duck> {
    /// Create [`RunScriptOptions`] from a given [`Path`] and [`ArgMatches`].
    pub fn from_path_and_matches(
        ctx: &'duck DuckContext,
        path: &'duck Path,
        matches: &ArgMatches,
    ) -> QuackResult<Self> {
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
        args: Vec<OsString>,
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
pub fn run_script<'duck>(rs_options: RunScriptOptions<'duck>) -> QuackResult<()> {
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
    let script_name = path
        .file_name()
        .context_internal("we assured that the path points to a file")?;
    let folder_path = path
        .parent()
        .context_internal("we assured that the path points to a file")?;
    let package = get_package(ctx, path, folder_path, global)?;
    let root_identity = package.package().as_a_local_identity()?;
    let (lock, venv, storage) = sync(
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

/// Loads the appropriate venv of the script.
fn get_package<'duck>(
    ctx: &'duck DuckContext,
    path: &Path,
    folder_path: &Path,
    global: bool,
) -> QuackResult<PackageContext<'duck>> {
    let possible_frontmatter = PackageContext::try_new_from_frontmatter(path.to_path_buf(), ctx)?;
    let possible_package =
        PackageLoader::find_from_directory(folder_path, ctx, AllowGlobalPackage::No);
    match (possible_frontmatter, possible_package, global) {
        // Scripts with frontmatters cannot be inside packages nor be run with `global` flag.
        (Some(_), Ok(_), _) => qp_bail!("scripts inside packages cannot have frontmatters"),
        (Some(_), _, true) => {
            qp_bail!("script with a frontmatter cannot be run with `global` flag")
        }
        (Some(frontmatter), Err(_), false) => Ok(frontmatter),

        // `global` forces the script to be run in the global venv, even if it is inside a package.
        (None, _, true) => PackageLoader::global_package(ctx),
        // If script does not belong to a package, default to global venv.
        (None, Err(_), false) => PackageLoader::global_package(ctx),

        (None, Ok(pcx), false) => Ok(pcx),
    }
}
