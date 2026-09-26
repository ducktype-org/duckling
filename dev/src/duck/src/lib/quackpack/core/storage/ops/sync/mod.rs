//! Synchronize a given venv.
//! This includes: creating a venv, resolving dependencies, downloading them.
use std::io;
use std::path::PathBuf;

use chrono::Utc;
use futures::executor::block_on;
use load_deps::{LoadedFreezePackages, load_packages_in_freeze};
use tracing::{debug, error, warn};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::lints::emit_warnings_and_run_lint_passes;
use crate::quackpack::core::script::Script;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::{Solver, SolverAnswer};
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::storage::git_access::StorageGitAccess;
use crate::quackpack::core::storage::locks::TrySyncLock;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::{Venv, VenvData};
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::quackpack::core::{
    AnyPackage, Package, PackageContext, PackageId, PackageLoader, VenvConfig, storage,
};
use crate::util::Pluralize;
use crate::util::error::MessageError;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext};

pub mod load_deps;

#[derive(Debug, Clone, Copy)]
/// Options passed to [`sync`].
pub struct StorageSyncOptions {
    /// Overwrite any existing venvs.
    pub overwrite: bool,
    /// Assume, that freezefile doesn't change.
    pub frozen: bool,
    /// Disallow any errors in foreign packages' manifests.
    pub strict_errors: bool,
}

#[derive(Debug)]
pub struct SyncOutput {
    pub new_freeze: SolverFreeze,
    pub loaded_packages: Vec<(PackageId, AnyPackage)>,
    pub sync_lock: TrySyncLock,
    pub new_venv: Venv,
    pub storage: Storage,
}

/// Synchronize virtual environment for package, and return information required to build it.
#[tracing::instrument(skip_all)]
pub fn sync(pcx: &PackageContext<'_>, options: StorageSyncOptions) -> QuackResult<SyncOutput> {
    debug!(root = %pcx.package().root().display(), ?options);
    emit_warnings_and_run_lint_passes(pcx)?;
    pcx.ctx().info(format!(
        "starting synchronization of the {}",
        pcx.package().display()
    ))?;
    let venv_config = pcx.package().venv();
    let storage = Storage::new(venv_config.storage_path());
    let fetcher = Fetcher::new(pcx.ctx())?;
    let git_access = StorageGitAccess::new(&storage);
    let expose_freezefile = venv_config.expose_freezefile();
    let user_exposed_freeze = load_external_freezefile(pcx, expose_freezefile)?;
    let id = pcx.to_venv_id();

    // First context is for IO results, second for unpacking Option (None = would block).
    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)
        .context("failed to acquire try sync lock")?
        .with_context(|| format!("another synchronization operation is ongoing in venv `{id}`"))?;

    let venv = Venv::fix_and_load(&storage, id, pcx.ctx())?;

    if !options.overwrite {
        check_if_overwrites(pcx, venv.as_ref(), id)?;
    }

    let input_freeze = user_exposed_freeze
        .as_ref()
        .or(venv.as_ref().map(|venv| venv.data().freeze()));

    let solver_answer = get_solver_answer(
        pcx,
        &fetcher,
        git_access,
        input_freeze,
        SolverMode::from(options),
    )?;
    let pkgs: Vec<PackageId> = solver_answer
        .new_freeze
        .package_freezes
        .keys()
        .copied()
        .collect();
    let new_freeze = solver_answer.new_freeze.generate_storage_freeze()?;

    let LoadedFreezePackages {
        pkgs,
        freshly_downloaded_num,
        already_present_num,
    } = load_packages_in_freeze(&storage, &fetcher, pkgs)?;
    make_after_fetch_message(pcx.ctx(), already_present_num, freshly_downloaded_num)?;

    let venv = update_venv(venv, venv_config, new_freeze, id, pcx);
    venv.save_to(&storage, pcx.ctx())?;

    if expose_freezefile && !options.frozen {
        let json = serde_json::to_string_pretty(venv.data().freeze())?;
        freeze_name(
            pcx.package()
                .try_get_package()
                .context_internal("`expose-freezefile` set on a script")?,
        )
        .write(json)?;
    }
    make_success_message(pcx, id)?;
    Ok(SyncOutput {
        new_freeze: solver_answer.new_freeze,
        loaded_packages: pkgs,
        sync_lock: _sync_lock,
        new_venv: venv,
        storage,
    })
}

/// Helper for [`sync`].
/// Checks if the venv for which the sync is run was previously synced from a different location,
/// and there is a manifest in that location.
/// This would override that manifest's venv.
#[tracing::instrument(skip_all, fields(%id))]
fn check_if_overwrites(
    pcx: &PackageContext<'_>,
    venv: Option<&Venv>,
    id: VenvId,
) -> QuackResult<()> {
    let Some(venv) = venv else { return Ok(()) };
    if pcx.package().root() == venv.data().last_known_location()
        || !venv.data().last_known_location().exists()
    {
        return Ok(());
    }
    let dir = venv.data().last_known_location();
    let package = PackageLoader::find_at_exact_directory(dir, pcx.ctx());
    let (replaces, context) = match package {
        Ok(package) => {
            let replaces = package.to_venv_id() == id && !id.is_global();
            let context = if replaces {
                Some(format!(
                    "synchronizing the package at `{}` would overwrite the venv of the package at `{}`",
                    pcx.package().root().display(),
                    dir.display()
                ))
            } else {
                None
            };
            (replaces, context)
        }
        Err(e) => {
            error!(path = %dir.display(), error = %e, "failed to load the package");
            if let Some(io_error) = e.downcast_ref_in_chain::<io::Error>() {
                // Maybe we missed something, check, if package has been moved.
                let replaces = ![io::ErrorKind::NotFound, io::ErrorKind::NotADirectory]
                    .contains(&io_error.kind());
                (replaces, None)
            } else {
                // Other error, maybe we failed to deserialize?
                // Safely assume, that package still exists.
                let context = format!(
                    "failed to load a package at `{}`, assuming it still exists with the name `{id}`",
                    dir.display()
                );
                (true, Some(context))
            }
        }
    };
    if replaces {
        let mut error = QuackError::hint("use `--overwrite` to force an overwrite");
        let same_ids_message = format!("the packages share the same name `{id}`");
        error = error.context(MessageError::new(same_ids_message));
        if let Some(context) = context {
            error = error.context(MessageError::new(context));
        }
        let tried_to_override_message =
            "tried to overwrite an existing virtual environment from another location";
        error = error.context(MessageError::new(tried_to_override_message));
        Err(error)
    } else {
        Ok(())
    }
}

/// Helper for [`sync`].
/// Generates freeze filename for the synced package.
fn freeze_name(package: &Package) -> PathBuf {
    package.root_directory().join(PackageLoader::FREEZE_NAME)
}

/// Helper for [`sync`].
/// If there is a freeze in the venv's root directory, deserializes it.
fn load_external_freezefile(
    pcx: &PackageContext<'_>,
    is_exposed: bool,
) -> QuackResult<Option<storage::freeze::VenvFreeze>> {
    if !is_exposed {
        return Ok(None);
    }
    let freeze_path = freeze_name(
        pcx.package()
            .try_get_package()
            .context_internal("`expose-freezefile` set on a script")?,
    );
    if !freeze_path.is_file() {
        return Ok(None);
    }
    let content = freeze_path.read_to_string()?;

    serde_json::from_str(&content)
        .with_context(|| format!("malformed freezefile `{}`", freeze_path.display()))
        .map(Some)
}

/// Helper for [`sync`].
/// Creates [`Solver`] and runs it.
#[tracing::instrument(skip_all)]
fn get_solver_answer(
    pcx: &PackageContext<'_>,
    fetcher: &Fetcher<'_>,
    git_access: StorageGitAccess<'_>,
    input_freeze: Option<&VenvFreeze>,
    mode: SolverMode,
) -> QuackResult<SolverAnswer> {
    debug!(?mode);
    let solver = Solver::new(pcx, input_freeze, fetcher, git_access, mode);
    block_on(solver.solve())
}

fn make_after_fetch_message(
    ctx: &DuckContext,
    already_present: usize,
    downloaded: usize,
) -> QuackResult<()> {
    let total = already_present + downloaded;
    ctx.info(format!(
        "loaded source code{} of {} package{}, {} {} downloaded, {} {} already present",
        total.s_if_plural(),
        total,
        total.s_if_plural(),
        downloaded,
        downloaded.was_or_were(),
        already_present,
        already_present.was_or_were()
    ))
}

/// Helper for [`sync`].
/// Updates the data of the venv.
fn update_venv(
    venv: Option<Venv>,
    venv_config: &VenvConfig,
    new_freeze: VenvFreeze,
    id: VenvId,
    pcx: &PackageContext<'_>,
) -> Venv {
    let now = Utc::now();
    if let Some(mut venv) = venv {
        let data = venv.data_mut();
        data.set_last_synchronization(now);
        data.set_freeze(new_freeze);
        data.set_last_known_location(pcx.package().root().to_path_buf());
        data.set_ephemeral(venv_config.ephemeral());
        venv
    } else {
        let data = VenvData::new(
            new_freeze,
            venv_config.ephemeral(),
            pcx.package().root().to_path_buf(),
            now,
            now,
        );
        Venv::new(id, data)
    }
}

/// Helper for [`sync`].
/// Prints to the user a message that synchronization was successful.
fn make_success_message(pcx: &PackageContext<'_>, id: VenvId) -> QuackResult<()> {
    match pcx.package() {
        AnyPackage::Script(Script::Standalone(script)) => pcx.ctx().info(format!(
            "successfully synchronized the venv of the script with a frontmatter at `{}`",
            script.frontmatter().script_file().display()
        )),
        AnyPackage::Package(_) | AnyPackage::Script(Script::Associated(_)) => pcx
            .ctx()
            .info(format!("successfully synchronized venv `{id}`")),
    }
}
