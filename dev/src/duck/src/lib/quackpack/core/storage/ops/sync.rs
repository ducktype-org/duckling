//! Synchronize a given venv.
//! This includes: creating a venv, resolving dependencies, downloading them.
use std::fs::File;
use std::io;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

use flate2::read::GzDecoder;
use tar::Archive;
use tracing::{debug, error};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::fetcher::types::PackageWithUrl;
use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::{ShouldRunSolverEngine, SolverAnswer, SolverGathererData};
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::storage::git_access::StorageGitAccess;
use crate::quackpack::core::storage::locks::TrySyncLock;
use crate::quackpack::core::storage::package_id::{GitId, RegistryId};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::{Venv, VenvData};
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::quackpack::core::{
    AnyPackage, GitReference, Package, PackageContext, PackageLoader, storage,
};
use crate::quackpack::util::with_version::WithVersion;
use crate::util::error::MessageError;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail};

const MAX_BLOB_RETRY_COUNT: i32 = 3;

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
    let venv_config = pcx.venv_config();
    let storage_localization = venv_config
        .storage_path()?
        .map(Path::to_path_buf)
        .unwrap_or_else(|| pcx.ctx().default_storage_root().into_not_locked_path());
    let storage = Storage::new(storage_localization);
    let mut fetcher = Fetcher::new(pcx.ctx())?;
    let mut git_access = StorageGitAccess::new(&storage);
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
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
        &mut fetcher,
        &mut git_access,
        input_freeze,
        SolverMode::from(options),
    )?;
    let pkgs: Vec<WithVersion<FullIdentity>> = solver_answer
        .new_freeze
        .package_freezes
        .keys()
        .copied()
        .collect();
    let new_freeze = solver_answer.new_freeze.generate_storage_freeze()?;

    let _was_anything_installed =
        fetch_source_codes(&storage, &mut fetcher, &mut git_access, pkgs)?;

    let now = SystemTime::now();
    let venv = if let Some(mut venv) = venv {
        let data = venv.data_mut();
        data.set_last_modification(now);
        data.set_freeze(new_freeze);
        data.set_last_known_directory(pcx.package().root().to_path_buf());
        venv
    } else {
        let data = VenvData::new(
            new_freeze,
            venv_config.is_ephemeral()?,
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
                .context_internal("expose_freezefile set on frontmatter pseudo package")?,
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
fn check_if_overwrites(
    pcx: &PackageContext<'_>,
    venv: Option<&Venv>,
    id: VenvId,
) -> QuackResult<()> {
    let Some(venv) = venv else { return Ok(()) };
    if pcx.package().root() == venv.data().last_known_directory()
        || !venv.data().last_known_directory().exists()
    {
        return Ok(());
    }
    let dir = venv.data().last_known_directory();
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
            error!(path = %dir.display(), "failed to load the package: {e} ({e:?})");
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
            .context_internal("expose_freezefile set on frontmatter pseudo package")?,
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
    fetcher: &mut Fetcher<'_>,
    git_access: &mut StorageGitAccess<'_>,
    input_freeze: Option<&VenvFreeze>,
    mode: SolverMode,
) -> QuackResult<SolverAnswer> {
    debug!(?mode);
    let root_origin = FullOrigin::for_local(pcx.package().root())?;
    let root_identity = FullIdentity::new(pcx.package().name(), root_origin);
    let root_pkg = WithVersion::new(root_identity, pcx.package().version());
    let solver_freeze = match input_freeze {
        Some(freeze) => SolverFreeze::try_from_venv_freeze(root_pkg, freeze)?,
        None => SolverFreeze::empty_with_root(root_pkg)?,
    };
    let solver = SolverGathererData::new(pcx, solver_freeze, mode)
        .context("failed to start gathering packages")?;
    let fetcher_lock = pcx.ctx().duck_home().open_fetcher_lockfile(pcx.ctx())?;
    let should_run_engine = solver.prepare_solving(fetcher, git_access)?;
    drop(fetcher_lock);
    debug!("will run solver engine: {should_run_engine}");
    match should_run_engine {
        ShouldRunSolverEngine::No(answer) => Ok(answer),
        ShouldRunSolverEngine::Yes(solver) => solver.solve(),
    }
}

/// Helper for [`sync`].
/// Fetches source codes of packages which have been decided to be part of the freeze,
/// but their source codes have not yet been fetched.
#[tracing::instrument(skip_all)]
fn fetch_source_codes(
    storage: &Storage,
    fetcher: &mut Fetcher<'_>,
    git_access: &mut StorageGitAccess<'_>,
    pkgs: Vec<WithVersion<FullIdentity>>,
) -> QuackResult<bool> {
    let mut was_anything_installed = false;
    let fetcher_lock = fetcher
        .ctx()
        .duck_home()
        .open_fetcher_lockfile(fetcher.ctx())?;
    for pkg in pkgs {
        was_anything_installed |= fetch_source_code(storage, fetcher, git_access, pkg)?;
    }
    drop(fetcher_lock);
    Ok(was_anything_installed)
}

/// Helper for [`fetch_source_codes`].
/// Fetches the source code of a package if it is not yet stored in the storage.
#[tracing::instrument(skip_all)]
fn fetch_source_code(
    storage: &Storage,
    fetcher: &mut Fetcher<'_>,
    git_access: &mut StorageGitAccess<'_>,
    pkg: WithVersion<FullIdentity>,
) -> QuackResult<bool> {
    debug!(?pkg);
    let identity = pkg.value();
    let origin = identity.origin();
    match origin.kind() {
        FullKind::Local => Ok(false),
        FullKind::Git { commit } => {
            let pkg_id = GitId::new(origin.url(), commit).into();
            if storage.is_package_stored(&pkg_id) {
                return Ok(false);
            }
            if git_access.is_stored(origin.url(), &commit) {
                storage.mark_as_stored(&pkg_id)?;
                return Ok(true);
            }
            fetcher.clone_from_git_to_directory(
                &origin.url(),
                GitReference::Rev(commit),
                &storage.pkg_dir(&pkg_id),
            )?;
            storage.mark_as_stored(&pkg_id)?;
            storage.pkg_dir(&pkg_id).try_fsync_dir()?;
            Ok(true)
        }
        FullKind::Registry => {
            let pkg_id = RegistryId::new(identity.name(), pkg.version(), origin.url()).into();
            if storage.is_package_stored(&pkg_id) {
                return Ok(false);
            }
            let mut successfully_fetched = false;
            let mut blob_path = PathBuf::new();
            for attempt in 1..=MAX_BLOB_RETRY_COUNT {
                match fetcher.fetch_package_blob(&PackageWithUrl {
                    name: identity.name(),
                    version: pkg.version(),
                    url: origin.url(),
                }) {
                    Ok(path) => {
                        blob_path = path;
                        successfully_fetched = true;
                        break;
                    }
                    Err(e) => {
                        if attempt != MAX_BLOB_RETRY_COUNT {
                            debug!(?pkg, "retrying fetch...");
                        } else {
                            debug!(?pkg, "failed to fetch: {e}");
                        }
                    }
                }
            }
            if !successfully_fetched {
                qp_bail!("Failed to fetch a package");
            }
            let pkg_dir = storage.pkg_dir(&pkg_id);
            if pkg_dir.exists() {
                pkg_dir.rm()?;
            }
            let file = File::open(blob_path)?;
            let decompressed = GzDecoder::new(file);
            let mut archive = Archive::new(decompressed);
            archive.unpack(pkg_dir.clone())?;
            storage.mark_as_stored(&pkg_id)?;
            pkg_dir.try_fsync_dir()?;
            Ok(true)
        }
    }
}

fn make_success_message(pcx: &PackageContext<'_>, id: VenvId) -> QuackResult<()> {
    match pcx.package() {
        AnyPackage::Frontmatter(frontmatter) => pcx.ctx().console().info(format!(
            "successfully synchronized the venv of the script with a frontmatter at `{}`",
            frontmatter.script_file().display()
        )),
        AnyPackage::Package(_) => pcx
            .ctx()
            .console()
            .info(format!("successfully synchronized venv `{id}`")),
    }
}
