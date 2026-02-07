use rustvil::fs::{PathExt, ShouldBlock};
use tracing::debug;

use crate::quackpack::core::storage;

use crate::{DuckCtx, QuackResult, QuackResultContext, StrId};
use std::collections::HashSet;
use std::fs::DirEntry;
use std::time::{Duration, SystemTime};
use std::{io, path::PathBuf};
use storage::IdOrPackage;
use storage::paths::StoragePaths;
use storage::{locks, paths, venv};

#[derive(Debug)]
pub struct CleanOutput {
    pub removed_vevns: Vec<StrId>,
    pub removed_packages: Vec<PathBuf>,
}

/// Delete a virtual environment from storage.
pub fn delete_venv(ctx: &DuckCtx, venv: IdOrPackage<'_>) -> QuackResult<()> {
    debug!("deleting venv `{venv:?}`");
    let storage = paths::StoragePaths::new(ctx.duck_home());
    let venv_id = venv.venv_id();

    let _sync_lock = {
        let mut would_block = false;
        locks::TrySyncLock::new(&storage, venv_id)
            .inspect_err(|err| {
                if err.kind() == io::ErrorKind::WouldBlock {
                    would_block = true;
                }
            })
            .with_context(|| {
                if would_block {
                    format!("another syncrhonization operation is ongoin in venv `{venv_id}`")
                } else {
                    format!("failed to acquire a lock for venv `{venv_id}`")
                }
            })?
    };
    let _lock = storage.data_lock(venv_id).lock(ShouldBlock::Yes)?;
    let _ = storage.venv_dir(venv_id).rmtree();
    Ok(())
}

/// Remove orphaned packages and expired temporary virtual environments from storage.
pub fn clean_storage(ctx: &DuckCtx) -> QuackResult<CleanOutput> {
    let temporary_lifetime = ctx.duck_cfg().storage_tmp_lifetime()?;
    let storage = paths::StoragePaths::new(ctx.duck_home());
    let mut removed_vevns = vec![];
    let _lock = locks::CleanLock::new(&storage).context("failed to acquire a clean lock")?;
    let mut all_deps = HashSet::new();
    let now = SystemTime::now();
    let venvs = {
        let venvs = storage.iter_vens()?;
        venvs.into_iter().collect::<Result<Vec<_>, _>>()?
    };
    for venv in venvs {
        clean_venv_from_storage(
            venv,
            &storage,
            temporary_lifetime,
            now,
            &mut removed_vevns,
            &mut all_deps,
        )?;
    }
    locks::cleanup_locks(&storage)?;
    let all_pkgs = storage.iter_pkgs()?.collect::<Result<Vec<_>, _>>()?;
    let removed_pkgs = all_pkgs
        .into_iter()
        .flat_map(|pkg| {
            let venv_id: StrId = pkg.file_name().to_string_lossy().into_owned().into();
            if !all_deps.contains(&venv_id) {
                Some(pkg.path())
            } else {
                None
            }
        })
        .collect::<Vec<_>>();
    for pkg in removed_pkgs.iter() {
        pkg.rmtree()?;
    }
    Ok(CleanOutput {
        removed_vevns,
        removed_packages: removed_pkgs,
    })
}

fn clean_venv_from_storage(
    venv: DirEntry,
    storage: &StoragePaths,
    temporary_lifetime: Duration,
    now: SystemTime,
    removed_vevns: &mut Vec<StrId>,
    all_deps: &mut HashSet<StrId>,
) -> QuackResult<()> {
    let venv_id = venv.file_name().to_string_lossy().into_owned().into();
    let _lock = storage.data_lock(venv_id).lock(ShouldBlock::Yes)?;
    let data = venv::fix_and_load_venv(storage, venv_id)?;
    let Some(mut data) = data else {
        return Ok(());
    };
    let mut requires_save = false;
    // last_access can exceed current_time only if there was a system time change.
    // If ephemeral venv's previous last_access is far in the future, we may never
    // clean it. Choosing to truncate the last_access to the present time may instead
    // cause premature cleanups (when measured in real time), but that should
    // not be problem for ephemeral venv.
    if data.last_access() > now {
        data.set_last_access(now);
        requires_save = true;
    }
    if data.is_ephemeral() && data.last_access() + temporary_lifetime < now {
        debug!("removing venv `{venv_id}` from the shared storage");
        removed_vevns.push(venv_id);
        venv.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                venv.path().display()
            )
        })?;
        return Ok(());
    }
    if requires_save {
        venv::save_venv(storage, venv_id, &data)?;
    }
    all_deps.extend(data.freeze().dependencies().iter().filter_map(|dep| {
        if dep.source().is_local() {
            None
        } else {
            Some(dep.to_package_id().storage_name())
        }
    }));
    Ok(())
}
