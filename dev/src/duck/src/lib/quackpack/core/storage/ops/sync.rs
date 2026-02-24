use std::{collections::HashMap, fs::File, path::PathBuf, sync::Arc, time::SystemTime};

use async_scoped::TokioScope;
use flate2::read::GzDecoder;
use tar::Archive;
use tokio::sync::Mutex;

use crate::{
    DuckCtx, QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    quackpack::{
        core::{
            BranchOrTag, Git, Package, PackageCtx, PackageLoader, Solver, SolverMode,
            fetcher::{Fetcher, types::PackageWithUrl},
            git_access::GitAccess,
            solver_freeze::SolverFreeze,
            storage::{
                git_access::StorageGitAccess,
                locks::{CompileLock, TrySyncLock},
                package_id::{GitId, PackageId, RegistryId},
                paths::Storage,
                venv::{Venv, VenvData},
                venv_id::ToVenvId,
            },
            types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
        },
        util::async_helpers::{extract_single_item_from_vec, unpack_tokio_scoped_vector},
    },
    util_common::path_ops_ext::{PathOpsExt, ShouldBlock},
};

use crate::quackpack::core::storage;

const MAX_BLOB_RETRY_COUNT: i32 = 3;

#[derive(Debug, Default, Clone, Copy)]
pub struct SyncOptions {
    pub overwrite: bool,
    pub frozen: bool,
    pub offline: bool,
}

/// Synchronize virtual environment for package, and return information required to build it.
///
/// Why is it safe:
///
/// Even though we drop `SyncLock`, we block cleanups from happening. And because solver only
/// returns *new* packages to install, we never remove nor overwrite anything in [`sync`],
/// therefore we can drop `SyncLock`.
pub fn sync(
    ctx: &DuckCtx,
    pkg_ctx: &PackageCtx,
    options: SyncOptions,
) -> QuackResult<(TrySyncLock, Venv, Storage)> {
    let storage = Storage::new(ctx.duck_home());
    let fetcher = Fetcher::new(ctx)?;
    let git_access = Arc::new(Mutex::new(StorageGitAccess::new(&storage, HashMap::new())));
    let venv_config = pkg_ctx.venv_config();
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let user_exposed_freeze = load_external_freezefile(pkg_ctx, expose_freezefile)?;
    let id = pkg_ctx.to_venv_id();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)
        .context("failed to acquire try sync lock")?;
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;

    let venv = Venv::fix_and_load(&storage, id)?;
    drop(data_lock);
    let input_freeze = user_exposed_freeze
        .as_ref()
        .or(venv.as_ref().map(|venv| venv.data().freeze()));
    let root_pkg = ExpandedPackage {
        location: InternedExpandedLocation::new(ExpandedLocation::Local {
            absolute_path: pkg_ctx.package().root_directory().to_path_buf(),
        }),
        version: None,
    };
    let solver_freeze = match input_freeze {
        Some(freeze) => SolverFreeze::try_from_venv_freeze(root_pkg, freeze)?,
        None => SolverFreeze::empty_with_root(root_pkg)?,
    };
    let solver = Solver::new(pkg_ctx, &fetcher, solver_freeze, SolverMode::Strict);
    let fetcher_lock = ctx
        .duck_home()
        .ensure_fetcher_lockfile()?
        .lock(ShouldBlock::Yes)?;
    let (_, results) = TokioScope::scope_and_block(|spawner| {
        spawner.spawn(async {
            let solver = solver.prepare_solving(git_access.clone()).await?;
            drop(fetcher_lock);
            solver.solve()
        });
    });
    let result = unpack_tokio_scoped_vector(results)?;
    let (new_solver_freeze, manifests) = extract_single_item_from_vec(result)??;
    let pkgs: Vec<ExpandedPackage> = new_solver_freeze.package_freezes.keys().copied().collect();
    let new_freeze = new_solver_freeze.generate_storage_freeze(&manifests)?;
    if !options.overwrite
        && let Some(venv) = venv
        && pkg_ctx.package().manifest_path() != venv.data().last_location()
        && venv.data().last_location().exists()
    {
        let replaces =
            PackageLoader::find_at_exact_directory(venv.data().last_location(), pkg_ctx.ctx())
                .map(|pkg| pkg.package().manifest().root_description().name() == id)
                .unwrap_or(false);
        if replaces {
            return Err(qp_err!(
                "tried to overwrite an existing virtual environment from another location"
            )
            .add_hint("use `--overwrite` to force an overwrite"));
        }
    }
    let fetcher_lock = ctx
        .duck_home()
        .ensure_fetcher_lockfile()?
        .lock(ShouldBlock::Yes)?;
    let (_, results) = TokioScope::scope_and_block(|spawner| {
        for pkg in pkgs {
            let tmp_pkg = Arc::new(pkg);
            let tmp_git_access = git_access.clone();
            spawner.spawn(async {
                fetch_source_code(&storage, &fetcher, tmp_git_access, tmp_pkg).await?;
                Ok::<(), QuackError>(())
            });
        }
    });
    drop(fetcher_lock);
    let results = unpack_tokio_scoped_vector(results)?;
    for result in results {
        result?;
    }
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let now = SystemTime::now();
    // @TODO: #1962 Skip this, if we have nothing to install.
    // Maybe we should bump `access_time` only in that case?
    let data = VenvData::new(
        new_freeze,
        venv_config.is_ephemeral()?,
        pkg_ctx.package().manifest_path().to_path_buf(),
        now,
    );
    let venv = Venv::new(id, data);
    venv.save_to(&storage)?;
    drop(data_lock);
    if expose_freezefile && !options.frozen {
        let json = serde_json::to_string_pretty(venv.data().freeze())?;
        freeze_name(pkg_ctx.package()).write(json)?;
    }
    Ok((_sync_lock, venv, storage))
}

fn freeze_name(package: &Package) -> PathBuf {
    package.root_directory().join(PackageLoader::FREEZE_NAME)
}

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

async fn fetch_source_code(
    storage: &Storage,
    fetcher: &Fetcher<'_>,
    git_access: Arc<Mutex<StorageGitAccess<'_>>>,
    pkg: Arc<ExpandedPackage>,
) -> QuackResult<()> {
    match pkg.location.as_ref() {
        ExpandedLocation::Local { absolute_path: _ } => {
            return Ok(());
        }
        ExpandedLocation::Git { url, commit } => {
            let pkg_id = PackageId::Git(GitId::new(url.clone(), *commit));
            if storage.is_package_stored(&pkg_id) {
                return Ok(());
            }
            let git_access = git_access.lock().await;
            if git_access.is_stored(url.clone(), *commit) {
                storage.mark_as_stored(&pkg_id)?;
                return Ok(());
            }
            fetcher
                .clone_from_git_to_directory(
                    &Git::new(url.clone(), BranchOrTag::Default, Some(*commit)),
                    &storage.pkg_dir(&pkg_id),
                )
                .await?;
            storage.mark_as_stored(&pkg_id)?;
            storage.pkg_dir(&pkg_id).try_fsync_dir()?;
            return Ok(());
        }
        ExpandedLocation::Registry { url, real_name } => {
            let Some(version) = pkg.version else {
                qp_bail_internal!("Registry package without version");
            };
            let pkg_id = PackageId::Registry(RegistryId::new(*real_name, version, url.clone()));
            if storage.is_package_stored(&pkg_id) {
                return Ok(());
            }
            let mut succesfully_fetched = false;
            let mut blob_path = PathBuf::new();
            for _ in 0..MAX_BLOB_RETRY_COUNT {
                if let Ok(path) = fetcher
                    .fetch_package_blob(&PackageWithUrl {
                        id: *real_name,
                        version,
                        url: url.clone(),
                    })
                    .await
                {
                    blob_path = path;
                    succesfully_fetched = true;
                    break;
                }
            }
            if !succesfully_fetched {
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
        }
    }
    Ok(())
}
