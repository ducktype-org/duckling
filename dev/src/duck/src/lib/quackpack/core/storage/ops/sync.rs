//! Synchronize a given venv.
//! This includes: creating a venv, resolving dependencies, downloading them.
use std::cell::RefCell;
use std::fs::File;
use std::io;
use std::path::{Path, PathBuf};

use chrono::Utc;
use flate2::read::GzDecoder;
use futures::executor::block_on;
use futures::future::join_all;
use tar::Archive;
use tracing::{debug, error, warn};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::fetcher::types::PackageWithUrl;
use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::core::script::Script;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::{ShouldRunSolverEngine, SolverAnswer, SolverGathererData};
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::storage::git_access::StorageGitAccess;
use crate::quackpack::core::storage::locks::TrySyncLock;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::{Venv, VenvData};
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::quackpack::core::{
    AnyPackage, GitReference, Package, PackageContext, PackageId, PackageLoader, storage,
};
use crate::util::error::{ErrorsLogger, MessageError};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal};

// We want at most 3 calls, therefore we retry 2 times.
const MAX_BLOB_RETRY_COUNT: u32 = 2;

#[derive(Debug, Clone, Copy)]
/// A marker struct to indicate a successful fetch.
struct SuccessfullyFetchedPackage;

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

/// Synchronize virtual environment for package, and return information required to build it.
#[tracing::instrument(skip_all)]
pub fn sync(
    pcx: &PackageContext<'_>,
    options: StorageSyncOptions,
) -> QuackResult<(TrySyncLock, Venv, Storage)> {
    debug!(root = %pcx.package().root().display(), ?options);
    pcx.ctx().console().info(format!(
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

    let _fetched_count = fetch_source_codes(&storage, &fetcher, git_access, pkgs)?;

    let now = Utc::now();
    let venv = if let Some(mut venv) = venv {
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
    };
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
    Ok((_sync_lock, venv, storage))
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
/// Prepares the input and runs [`SolverGathererData::prepare_solving`].
#[tracing::instrument(skip_all)]
fn get_solver_answer(
    pcx: &PackageContext<'_>,
    fetcher: &Fetcher<'_>,
    git_access: StorageGitAccess<'_>,
    input_freeze: Option<&VenvFreeze>,
    mode: SolverMode,
) -> QuackResult<SolverAnswer> {
    debug!(?mode);
    pcx.ctx()
        .console()
        .info("starting solving the dependency graph")?;
    let root_origin = FullOrigin::for_local(pcx.package().root())?;
    let root_identity = FullIdentity::new(pcx.package().name(), root_origin);
    let root_pkg = PackageId::new(root_identity, pcx.package().version());
    let solver_freeze = match input_freeze {
        Some(freeze) => SolverFreeze::try_from_venv_freeze(root_pkg, freeze)?,
        None => SolverFreeze::empty_with_root(root_pkg)?,
    };
    let solver = SolverGathererData::new(pcx, solver_freeze, mode)
        .context("failed to start gathering packages")?;
    let fetcher_lock = pcx.ctx().duck_home().open_fetcher_lockfile(pcx.ctx())?;
    let should_run_engine = block_on(solver.prepare_solving(fetcher, &git_access))?;
    drop(fetcher_lock);
    debug!(%should_run_engine);
    match should_run_engine {
        ShouldRunSolverEngine::No(answer) => {
            pcx.ctx()
                .console()
                .info("no need to run the solver engine")?;
            Ok(answer)
        }
        ShouldRunSolverEngine::Yes(solver) => {
            pcx.ctx().console().info("starting the solver engine")?;
            solver.solve()
        }
    }
}

/// Helper for [`sync`].
/// Fetches source codes of packages which have been decided to be part of the freeze,
/// but their source codes have not yet been fetched.
/// Returns the number of fetched packages.
#[tracing::instrument(skip_all)]
fn fetch_source_codes(
    storage: &Storage,
    fetcher: &Fetcher<'_>,
    git_access: StorageGitAccess<'_>,
    pkgs: Vec<PackageId>,
) -> QuackResult<usize> {
    if pkgs.is_empty() {
        warn!("requested a download of 0 packages");
        return Ok(0);
    }
    let count = pkgs.len();
    let suffix = if count == 1 { "" } else { "s" };
    fetcher
        .ctx()
        .console()
        .info(format!("starting fetching {count} package{suffix}"))?;
    let fetcher_lock = fetcher
        .ctx()
        .duck_home()
        .open_fetcher_lockfile(fetcher.ctx())?;
    let logger = RefCell::new(ErrorsLogger::default());
    let fetches = pkgs
        .into_iter()
        .map(|pkg| fetch_source_code(storage, fetcher, git_access, pkg, &logger));
    let fetches = block_on(join_all(fetches));
    drop(fetcher_lock);
    bail_if_failed_to_fetch(fetcher.ctx(), logger.take())?;
    Ok(fetches.into_iter().flatten().count())
}

/// Bail if we failed to download some package.
fn bail_if_failed_to_fetch(ctx: &DuckContext, logger: ErrorsLogger) -> QuackResult<()> {
    if logger.is_empty() {
        return Ok(());
    }
    let failed_count = logger.logged_errors();
    let failed_suffix = if failed_count == 1 { "" } else { "s" };
    for fail in logger {
        ctx.error_console().error(fail)?;
    }
    qp_bail!("failed to fetch {failed_count} package{failed_suffix}")
}

/// Helper for [`fetch_source_codes`].
/// Fetches the source code of a package if it is not yet stored in the storage.
#[tracing::instrument(skip_all, fields(?pkg))]
async fn fetch_source_code(
    storage: &Storage,
    fetcher: &Fetcher<'_>,
    git_access: StorageGitAccess<'_>,
    pkg: PackageId,
    logger: &RefCell<ErrorsLogger>,
) -> Option<SuccessfullyFetchedPackage> {
    debug!("fetching package");
    macro_rules! log {
        ($e:expr) => {
            logger.borrow_mut().log_result($e)?
        };
    }
    let url = pkg.url();
    match pkg.kind() {
        FullKind::Local => None,
        FullKind::Git { commit } => {
            if storage.is_stored_git(url, &commit) {
                return None;
            }
            if git_access.is_stored(url, &commit) {
                log!(storage.mark_as_stored(pkg).with_context(|| {
                    format!(
                        "failed to store a cloned repository of a package `{}`",
                        pkg.value().as_identity()
                    )
                }));
                return Some(SuccessfullyFetchedPackage);
            }
            log!(fetcher.clone_from_git_to_directory(
                &url,
                GitReference::Rev(commit),
                &storage.git_dir(url, &commit),
            ));
            log!(mark_cloned_git_as_stored(pkg, storage).with_context(|| {
                format!(
                    "failed to store a cloned repository of a package `{}`",
                    pkg.value().as_identity()
                )
            }));
            Some(SuccessfullyFetchedPackage)
        }
        FullKind::Registry => {
            if storage.is_package_stored(pkg) {
                return None;
            }
            let fetcher_package = PackageWithUrl {
                name: pkg.name(),
                version: pkg.version(),
                url: pkg.url(),
            };
            let maybe_blob_path = fetcher
                .fetch_package_blob_with_retries(fetcher_package, MAX_BLOB_RETRY_COUNT)
                .await;
            let blob_path = log!(maybe_blob_path);
            log!(
                unpack_package_blob(pkg, storage, &blob_path).with_context(|| format!(
                    "failed to unpack a compressed source code `{}` of a package `{}`",
                    blob_path.display(),
                    pkg.value().as_identity()
                ))
            );
            Some(SuccessfullyFetchedPackage)
        }
    }
}

/// Unpack a fetched package blob.
fn unpack_package_blob(pkg: PackageId, storage: &Storage, blob_path: &Path) -> QuackResult<()> {
    let pkg_dir = storage.pkg_dir(pkg);
    if pkg_dir.exists() {
        pkg_dir.rm()?;
    }
    let file = File::open(blob_path)
        .with_context(|| format!("failed to open `{}`", blob_path.display()))?;
    let decompressed = GzDecoder::new(file);
    let mut archive = Archive::new(decompressed);
    archive.unpack(&pkg_dir).with_context(|| {
        format!(
            "failed to unpack `{}` to `{}`",
            blob_path.display(),
            pkg_dir.display()
        )
    })?;
    storage.mark_as_stored(pkg)?;
    pkg_dir.try_fsync_dir()?;
    Ok(())
}

/// Mark a cloned git as stored.
fn mark_cloned_git_as_stored(pkg: PackageId, storage: &Storage) -> QuackResult<()> {
    let FullKind::Git { commit } = pkg.kind() else {
        qp_bail_internal!("attempted to store not a git package: {pkg:?}")
    };

    let url = pkg.url();
    storage.mark_as_stored(pkg)?;
    storage.git_dir(url, &commit).try_fsync_dir()?;
    Ok(())
}

fn make_success_message(pcx: &PackageContext<'_>, id: VenvId) -> QuackResult<()> {
    match pcx.package() {
        AnyPackage::Script(Script::Standalone(script)) => pcx.ctx().console().info(format!(
            "successfully synchronized the venv of the script with a frontmatter at `{}`",
            script.frontmatter().script_file().display()
        )),
        AnyPackage::Package(_) | AnyPackage::Script(Script::Associated(_)) => pcx
            .ctx()
            .console()
            .info(format!("successfully synchronized venv `{id}`")),
    }
}
