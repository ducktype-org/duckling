//! Synchronize a given venv.
//! This includes: creating a venv, resolving dependencies, downloading them.
use std::{fs::File, path::PathBuf, time::SystemTime};

use flate2::read::GzDecoder;
use tar::Archive;

use crate::{
    QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal, qp_err,
    quackpack::{
        core::{
            BranchOrTag, Git, Package, PackageCtx, PackageLoader, ShouldRunSolverEngine,
            SolverAnswer, SolverGathererData,
            fetcher::{Fetcher, types::PackageWithUrl},
            git_access::GitAccess,
            solver_freeze::SolverFreeze,
            solver_mode::SolverMode,
            storage::{
                freeze::VenvFreeze,
                git_access::StorageGitAccess,
                locks::TrySyncLock,
                package_id::{GitId, PackageId, RegistryId},
                paths::Storage,
                venv::{Venv, VenvData},
                venv_id::ToVenvId,
            },
            types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
        },
        subcommands::sync::SyncOptions,
    },
    util_common::path_ops_ext::{PathOpsExt, ShouldBlock},
};

use crate::quackpack::core::storage;

const MAX_BLOB_RETRY_COUNT: i32 = 3;

/// Synchronize virtual environment for package, and return information required to build it.
pub fn sync(
    package: &PackageCtx<'_>,
    options: SyncOptions,
) -> QuackResult<(TrySyncLock, Venv, Storage)> {
    let venv_config = package.venv_config();
    let storage_localization = venv_config
        .storage_path()?
        .unwrap_or(package.ctx().default_storage_root());
    let storage = Storage::new(storage_localization);
    let mut fetcher = Fetcher::new(package.ctx())?;
    let mut git_access = StorageGitAccess::new(&storage);
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let user_exposed_freeze = load_external_freezefile(package, expose_freezefile)?;
    let id = package.to_venv_id();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)
        .context("failed to acquire try sync lock")?;

    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let venv = Venv::fix_and_load(&storage, id)?;
    drop(data_lock);

    if !options.overwrite {
        check_if_overwrites(package, venv.as_ref(), id)?;
    }

    let input_freeze = user_exposed_freeze
        .as_ref()
        .or(venv.as_ref().map(|venv| venv.data().freeze()));

    let solver_answer = get_solver_answer(
        package,
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

    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let now = SystemTime::now();
    let data = VenvData::new(
        new_freeze,
        venv_config.is_ephemeral()?,
        package.package().manifest_path().to_path_buf(),
        now,
    );
    let venv = Venv::new(id, data);
    venv.save_to(&storage)?;
    drop(data_lock);

    if expose_freezefile && !options.frozen {
        let json = serde_json::to_string_pretty(venv.data().freeze())?;
        freeze_name(package.package()).write(json)?;
    }
    Ok((_sync_lock, venv, storage))
}

/// Helper for [`sync`].
/// Checks if the venv for which the sync is run was previously synced from a different location,
/// and there is a manifest in that location.
/// This would override that manifest's venv.
fn check_if_overwrites(
    pkg_ctx: &PackageCtx<'_>,
    venv: Option<&Venv>,
    id: StrId,
) -> QuackResult<()> {
    let Some(venv) = venv else { return Ok(()) };
    if pkg_ctx.package().manifest_path() != venv.data().last_location()
        && venv.data().last_location().exists()
    {
        let replaces =
            PackageLoader::find_at_exact_directory(venv.data().last_location(), pkg_ctx.ctx())
                .map(|pkg| pkg.package().manifest().name() == id)
                .unwrap_or(false);
        if replaces {
            Err(
                qp_err!("tried to overwrite an existing virtual environment from another location")
                    .add_hint("use `--overwrite` to force an overwrite"),
            )
        } else {
            Ok(())
        }
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
    ctx: &PackageCtx,
    is_exposed: bool,
) -> QuackResult<Option<storage::freeze::VenvFreeze>> {
    if !is_exposed {
        return Ok(None);
    }
    let freeze_path = freeze_name(ctx.package());
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
fn get_solver_answer(
    package: &PackageCtx<'_>,
    fetcher: &mut Fetcher<'_>,
    git_access: &mut StorageGitAccess<'_>,
    input_freeze: Option<&VenvFreeze>,
    mode: SolverMode,
) -> QuackResult<SolverAnswer> {
    let root_pkg = ExpandedPackage {
        location: InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: package.package().root_directory().to_path_buf(),
        }),
        version: None,
    };
    let solver_freeze = match input_freeze {
        Some(freeze) => SolverFreeze::try_from_venv_freeze(root_pkg, freeze)?,
        None => SolverFreeze::empty_with_root(root_pkg)?,
    };
    let solver = SolverGathererData::new(package, solver_freeze, mode);
    let fetcher_lock = package
        .ctx()
        .duck_home()
        .ensure_fetcher_lockfile()?
        .lock(ShouldBlock::Yes)?;
    let should_run_engine = solver.prepare_solving(fetcher, git_access)?;
    drop(fetcher_lock);
    match should_run_engine {
        ShouldRunSolverEngine::No(answer) => Ok(answer),
        ShouldRunSolverEngine::Yes(solver) => solver.solve(),
    }
}

/// Helper for [`sync`].
/// Fetches source codes of packages which have been decided to be part of the freeze,
/// but their source codes have not yet been fetched.
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
fn fetch_source_code(
    storage: &Storage,
    fetcher: &mut Fetcher<'_>,
    git_access: &mut StorageGitAccess<'_>,
    pkg: ExpandedPackage,
) -> QuackResult<bool> {
    match pkg.location.as_ref() {
        ExpandedLocation::Local { absolute_path: _ } => Ok(false),
        ExpandedLocation::Git { url, commit } => {
            let pkg_id = PackageId::Git(GitId::new(url.clone(), *commit));
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
            let pkg_id = PackageId::Registry(RegistryId::new(*real_name, version, url.clone()));
            if storage.is_package_stored(&pkg_id) {
                return Ok(false);
            }
            let mut successfully_fetched = false;
            let mut blob_path = PathBuf::new();
            for _ in 0..MAX_BLOB_RETRY_COUNT {
                if let Ok(path) = fetcher.fetch_package_blob(&PackageWithUrl {
                    id: *real_name,
                    version,
                    url: url.clone(),
                }) {
                    blob_path = path;
                    successfully_fetched = true;
                    break;
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
