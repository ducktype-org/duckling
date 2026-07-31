//! Removing files from a storage.
use std::collections::HashSet;
use std::fs::DirEntry;
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};

use storage::paths::Storage;
use storage::{locks, paths};
use tracing::{debug, error, info, warn};

use crate::quackpack::core::storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug)]
/// An output of a [`clean_storage`].
pub struct CleanOutput {
    /// Ids of removed venvs.
    pub removed_venvs: Vec<VenvId>,
    /// Paths to the removed packages.
    pub removed_packages: Vec<PathBuf>,
}

/// Delete a virtual environment from storage.
pub fn delete_venv(ctx: &DuckContext, storage_root: &Path, venv: impl ToVenvId) -> QuackResult<()> {
    let storage = paths::Storage::new(storage_root);
    let venv_id = venv.to_venv_id();
    debug!(id = %venv_id, "deleting venv");

    let _sync_lock = {
        // First context is for IO results, second for unpacking Option (None = would block).
        let lock = locks::TrySyncLock::new(&storage, venv_id)
            .with_context(|| format!("failed to acquire a lock for venv `{venv_id}`"))?;
        let Some(lock) = lock else {
            qp_bail!("another synchronization operation is ongoing in venv `{venv_id}`")
        };
        lock
    };
    let data_lock = storage.data_locks().open_exclusive(venv_id, ctx)?;
    if !storage.venv_dir(venv_id).is_dir() {
        return Ok(());
    }
    storage.venv_dir(venv_id).rmtree()?;
    storage.sync_locks_path().join(venv_id).rm()?;
    data_lock.path().rm()?;
    info!(id = %venv_id, "deleted venv");
    ctx.console()
        .info(format!("successfully removed venv `{venv_id}`"))?;
    Ok(())
}

/// Remove orphaned packages and expired temporary virtual environments from storage.
pub fn clean_storage(ctx: &DuckContext, storage_root: &Path) -> QuackResult<CleanOutput> {
    debug!(root = %storage_root.display(), "cleaning storage");
    let temporary_lifetime = ctx.duck_cfg().storage_tmp_lifetime()?;
    let storage = paths::Storage::new(storage_root);
    let mut removed_venvs = vec![];
    let _lock = locks::CleanLock::new(&storage, ctx).context("failed to acquire a clean lock")?;
    let mut all_deps = HashSet::new();
    let now = SystemTime::now();
    let venvs = {
        let venvs = storage.iter_venvs()?;
        venvs.into_iter().collect::<Result<Vec<_>, _>>()?
    };
    debug!(?venvs, "removing venvs");
    for venv in venvs {
        clean_venv_from_storage(
            venv,
            &storage,
            temporary_lifetime,
            now,
            &mut removed_venvs,
            &mut all_deps,
            ctx,
        )?;
    }
    info!("removed venvs");
    locks::cleanup_locks(&storage)?;
    debug!(?all_deps, "used dependencies");
    let all_pkgs = storage.iter_pkgs()?.collect::<Result<Vec<_>, _>>()?;
    debug!(?all_pkgs, "all known packages");
    let packages_to_remove = all_pkgs
        .into_iter()
        .flat_map(|pkg| {
            let venv_id: StrId = pkg.file_name().into();
            if !all_deps.contains(venv_id.as_str()) {
                Some(pkg.path())
            } else {
                None
            }
        })
        .collect::<Vec<_>>();
    debug!(?packages_to_remove, "cleaning packages");
    for pkg in packages_to_remove.iter() {
        pkg.rmtree()?;
    }
    info!(?packages_to_remove, "cleaned packages");
    ctx.console().info("successfully cleaned the storage")?;
    Ok(CleanOutput {
        removed_venvs,
        removed_packages: packages_to_remove,
    })
}

/// Remove a single venv from a storage.
/// A helper for [`clean_storage`].
#[tracing::instrument(skip_all, fields(?temporary_lifetime))]
fn clean_venv_from_storage(
    dir: DirEntry,
    storage: &Storage,
    temporary_lifetime: Duration,
    now: SystemTime,
    removed_venvs: &mut Vec<VenvId>,
    all_deps: &mut HashSet<String>,
    ctx: &DuckContext,
) -> QuackResult<()> {
    let venv_id = dir.file_name().to_venv_id();
    debug!(id = %venv_id, "removing venv");
    let venv = Venv::fix_and_load(storage, venv_id, ctx)?;
    let Some(mut venv) = venv else {
        error!(id = %venv_id, "failed to fix and load venv");
        warn!(id = %venv_id, "will clean regardless of what");
        dir.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                dir.path().display()
            )
        })?;
        info!(id = %venv_id, "removed venv");
        removed_venvs.push(venv_id);
        return Ok(());
    };
    let mut requires_save = false;
    let data = venv.data_mut();
    // last_synchronization can exceed current_time only if there was a system time change.
    // If ephemeral venv's previous last_synchronization is far in the future, we may never
    // clean it. Choosing to truncate the last_access to the present time may instead
    // cause premature cleanups (when measured in real time), but that should
    // not be problem for ephemeral venv.
    if data.last_synchronization() > now {
        data.set_last_synchronization(now);
        requires_save = true;
        debug!(id = %venv_id, "requires_save, because it's too old");
    }
    let is_too_old = data.last_synchronization() + temporary_lifetime < now;
    let should_remove_venv = data.is_ephemeral() && is_too_old;
    debug!(
        id = %venv_id,
        ephemeral = %data.is_ephemeral(),
        last_synchronization = ?data.last_synchronization(),
        ?now,
        %is_too_old,
        %should_remove_venv,
    );
    if should_remove_venv {
        debug!(
        id = %venv_id,
            "removing venv from the shared storage, as it's too old and is ephemeral"
        );
        dir.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                dir.path().display()
            )
        })?;
        info!(id = %venv_id, "removed venv");
        removed_venvs.push(venv_id);
        return Ok(());
    }
    // We're done mutating data, let's make borrow checker happy.
    let data = venv.data();
    if requires_save {
        debug!(id = %venv_id, "saving venv");
        venv.save_to(storage, ctx)?;
    }
    all_deps.extend(data.freeze().dependencies().iter().filter_map(|dep| {
        if dep.identity().origin().kind().is_local() {
            None
        } else {
            Some(dep.to_package_id().storage_name())
        }
    }));
    Ok(())
}
