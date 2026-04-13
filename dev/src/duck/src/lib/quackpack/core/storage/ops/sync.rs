//! Synchronize a given venv.
//! This includes: creating a venv, resolving dependencies, downloading them.
use std::{fs::File, io, path::PathBuf, time::SystemTime};

use flate2::read::GzDecoder;
use tar::Archive;
use tracing::debug;

use crate::{
    QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    quackpack::core::{
        BranchOrTag, Git, Package, PackageContext, PackageLoader, ShouldRunSolverEngine,
        SolverAnswer, SolverGathererData,
        fetcher::{Fetcher, types::PackageWithUrl},
        git_access::GitAccess,
        solver_freeze::SolverFreeze,
        solver_mode::SolverMode,
        storage::{
            freeze::VenvFreeze,
            git_access::StorageGitAccess,
            locks::TrySyncLock,
            package_id::{GitId, RegistryId},
            paths::Storage,
            venv::{Venv, VenvData},
            venv_id::{ToVenvId, VenvId},
        },
        types_common::{ExpandedLocation, ExpandedPackage},
    },
    util::path_ops_ext::{PathOpsExt, ShouldBlock},
};

use crate::quackpack::core::storage;

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
    debug!(root = %pcx.package().root_directory().display(), ?options);
    let venv_config = pcx.venv_config();
    let storage_localization = venv_config
        .storage_path()?
        .unwrap_or(pcx.ctx().default_storage_root());
    let storage = Storage::new(storage_localization);
    let mut fetcher = Fetcher::new(pcx.ctx())?;
    let mut git_access = StorageGitAccess::new(&storage);
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let user_exposed_freeze = load_external_freezefile(pcx, expose_freezefile)?;
    let id = pcx.to_venv_id();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)
        .context("failed to acquire try sync lock")?;

    let venv = Venv::fix_and_load(&storage, id)?;

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
    let pkgs: Vec<ExpandedPackage> = solver_answer
        .new_freeze
        .package_freezes
        .keys()
        .copied()
        .collect();
    let new_freeze = solver_answer
        .new_freeze
        .generate_storage_freeze(&solver_answer.pkgs_manifests)?;

    let _was_anything_installed =
        fetch_source_codes(&storage, &mut fetcher, &mut git_access, pkgs)?;

    let now = SystemTime::now();
    let venv = if let Some(mut venv) = venv {
        let data = venv.data_mut();
        data.set_last_modification(now);
        data.set_freeze(new_freeze);
        data.set_last_known_directory(pcx.package().root_directory().to_path_buf());
        venv
    } else {
        let data = VenvData::new(
            new_freeze,
            venv_config.is_ephemeral()?,
            pcx.package().root_directory().to_path_buf(),
            now,
            now,
        );
        Venv::new(id, data)
    };
    venv.save_to(&storage)?;

    if expose_freezefile && !options.frozen {
        let json = serde_json::to_string_pretty(venv.data().freeze())?;
        freeze_name(pcx.package()).write(json)?;
    }
    pcx.ctx()
        .console()
        .info(format!("successfully synchronized venv `{id}`"));
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
    if pcx.package().root_directory() == venv.data().last_known_directory()
        || !venv.data().last_known_directory().exists()
    {
        return Ok(());
    }
    let package =
        PackageLoader::find_at_exact_directory(venv.data().last_known_directory(), pcx.ctx());
    let replaces = match package {
        Ok(package) => package.package().manifest().name() == id.name() && !id.is_global(),
        Err(e) => {
            if let Some(io_error) = e.downcast_ref_in_chain::<io::Error>() {
                // Maybe we missed something, check, if package has been moved.
                ![io::ErrorKind::NotFound, io::ErrorKind::NotADirectory].contains(&io_error.kind())
            } else {
                // Other error, maybe we failed to deserialize?
                // Safely assume, that package still exists.
                true
            }
        }
    };
    if replaces {
        Err(
            qp_err!("tried to overwrite an existing virtual environment from another location")
                .add_hint("use `--overwrite` to force an overwrite"),
        )
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
    let freeze_path = freeze_name(pcx.package());
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
    let root_pkg = ExpandedPackage {
        location: ExpandedLocation::Local {
            absolute_path: pcx.package().root_directory().to_path_buf(),
        }
        .into(),
        version: None,
    };
    let solver_freeze = match input_freeze {
        Some(freeze) => SolverFreeze::try_from_venv_freeze(root_pkg, freeze)?,
        None => SolverFreeze::empty_with_root(root_pkg)?,
    };
    let solver = SolverGathererData::new(pcx, solver_freeze, mode);
    let fetcher_lock = pcx
        .ctx()
        .duck_home()
        .ensure_fetcher_lockfile()?
        .lock(ShouldBlock::Yes)?;
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
    pkgs: Vec<ExpandedPackage>,
) -> QuackResult<bool> {
    let mut was_anything_installed = false;
    let fetcher_lock = fetcher
        .ctx()
        .duck_home()
        .ensure_fetcher_lockfile()?
        .lock(ShouldBlock::Yes)?;
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
    pkg: ExpandedPackage,
) -> QuackResult<bool> {
    debug!(?pkg);
    match pkg.location.as_ref() {
        ExpandedLocation::Local { absolute_path: _ } => Ok(false),
        ExpandedLocation::Git { url, commit } => {
            let pkg_id = GitId::new(url.clone(), *commit).into();
            if storage.is_package_stored(&pkg_id) {
                return Ok(false);
            }
            if git_access.is_stored(url.clone(), *commit) {
                storage.mark_as_stored(&pkg_id)?;
                return Ok(true);
            }
            fetcher.clone_from_git_to_directory(
                &Git::new(url.clone(), BranchOrTag::Default, Some(*commit)),
                &storage.pkg_dir(&pkg_id),
            )?;
            storage.mark_as_stored(&pkg_id)?;
            storage.pkg_dir(&pkg_id).try_fsync_dir()?;
            Ok(true)
        }
        ExpandedLocation::Registry { url, real_name } => {
            let Some(version) = pkg.version else {
                qp_bail_internal!("Registry package without version");
            };
            let pkg_id = RegistryId::new(*real_name, version, url.clone()).into();
            if storage.is_package_stored(&pkg_id) {
                return Ok(false);
            }
            let mut successfully_fetched = false;
            let mut blob_path = PathBuf::new();
            for attempt in 1..=MAX_BLOB_RETRY_COUNT {
                match fetcher.fetch_package_blob(&PackageWithUrl {
                    id: *real_name,
                    version,
                    url: url.clone(),
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
