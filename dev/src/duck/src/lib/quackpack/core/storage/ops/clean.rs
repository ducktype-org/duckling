//! Removing files from a storage.
use tracing::debug;

use crate::quackpack::core::storage;

use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::util::path_ops_ext::{PathOpsExt, ShouldBlock};
use crate::{DuckCtx, QuackResult, QuackResultContext, StrId};
use std::collections::HashSet;
use std::fs::DirEntry;
use std::path::Path;
use std::time::{Duration, SystemTime};
use std::{io, path::PathBuf};
use storage::paths::Storage;
use storage::{locks, paths};

#[derive(Debug)]
/// An output of a [`clean_storage`].
pub struct CleanOutput {
    /// Ids of removed venvs.
    pub removed_venvs: Vec<VenvId>,
    /// Paths to the removed packages.
    pub removed_packages: Vec<PathBuf>,
}

/// Delete a virtual environment from storage.
pub fn delete_venv(storage_root: &Path, venv: impl ToVenvId) -> QuackResult<()> {
    debug!("deleting venv `{}`", venv.to_venv_id());
    let storage = paths::Storage::new(storage_root);
    let venv_id = venv.to_venv_id();

    let _sync_lock = {
        let mut would_block = false;
        locks::TrySyncLock::new(&storage, venv_id)
            .inspect_err(|err| {
                if err
                    .downcast_ref_in_chain::<io::Error>()
                    .is_some_and(|io_err| io_err.kind() == io::ErrorKind::WouldBlock)
                {
                    would_block = true;
                }
            })
            .with_context(|| {
                if would_block {
                    format!("another synchronization operation is ongoing in venv `{venv_id}`")
                } else {
                    format!("failed to acquire a lock for venv `{venv_id}`")
                }
            })?
    };
    let _lock = storage
        .data_lock(venv_id)
        .lock(ShouldBlock::Yes)
        .with_context(|| {
            format!(
                "failed to acquire exclusive data lock for venv `{}`",
                venv_id
            )
        })?;
    if !storage.venv_dir(venv_id).is_dir() {
        return Ok(());
    }
    storage.venv_dir(venv_id).rmtree()?;
    storage.compile_lock(venv_id).rm()?;
    storage.sync_lock(venv_id).rm()?;
    storage.data_lock(venv_id).rm()?;
    Ok(())
}

/// Remove orphaned packages and expired temporary virtual environments from storage.
pub fn clean_storage(ctx: &DuckCtx, storage_root: &Path) -> QuackResult<CleanOutput> {
    debug!("cleaning storage");
    let temporary_lifetime = ctx.duck_cfg().storage_tmp_lifetime()?;
    let storage = paths::Storage::new(storage_root);
    let mut removed_venvs = vec![];
    let _lock = locks::CleanLock::new(&storage).context("failed to acquire a clean lock")?;
    let mut all_deps = HashSet::new();
    let now = SystemTime::now();
    let venvs = {
        let venvs = storage.iter_venvs()?;
        venvs.into_iter().collect::<Result<Vec<_>, _>>()?
    };
    debug!("iterating over all venvs: `{venvs:?}`");
    for venv in venvs {
        clean_venv_from_storage(
            venv,
            &storage,
            temporary_lifetime,
            now,
            &mut removed_venvs,
            &mut all_deps,
        )?;
    }
    locks::cleanup_locks(&storage)?;
    debug!("all stashed deps are `{all_deps:?}");
    let all_pkgs = storage.iter_pkgs()?.collect::<Result<Vec<_>, _>>()?;
    let pgks_to_remove = all_pkgs
        .into_iter()
        .flat_map(|pkg| {
            let venv_id: StrId = pkg.file_name().into();
            if !all_deps.contains(&venv_id) {
                Some(pkg.path())
            } else {
                None
            }
        })
        .collect::<Vec<_>>();
    for pkg in pgks_to_remove.iter() {
        pkg.rmtree()?;
    }
    Ok(CleanOutput {
        removed_venvs,
        removed_packages: pgks_to_remove,
    })
}

/// Remove a single venv from a storage.
/// A helper for [`clean_storage`].
fn clean_venv_from_storage(
    dir: DirEntry,
    storage: &Storage,
    temporary_lifetime: Duration,
    now: SystemTime,
    removed_venvs: &mut Vec<VenvId>,
    all_deps: &mut HashSet<StrId>,
) -> QuackResult<()> {
    let venv_id = dir.file_name().to_venv_id();
    let venv = Venv::fix_and_load(storage, venv_id)?;
    let Some(mut venv) = venv else {
        debug!("failed to fix and load venv `{venv_id}`");
        return Ok(());
    };
    let mut requires_save = false;
    let data = venv.data_mut();
    // last_modification can exceed current_time only if there was a system time change.
    // If ephemeral venv's previous last_modification is far in the future, we may never
    // clean it. Choosing to truncate the last_access to the present time may instead
    // cause premature cleanups (when measured in real time), but that should
    // not be problem for ephemeral venv.
    if data.last_modification() > now {
        data.set_last_modification(now);
        requires_save = true;
        debug!("venv `{venv_id}` requires_save, because it's too old");
    }
    debug!(
        "venv's `{venv_id}` (ephemeral: {}) last modification is `{:?}`, now is `{now:?}`",
        data.is_ephemeral(),
        data.last_modification()
    );
    let is_too_old = data.last_modification() + temporary_lifetime < now;
    let should_remove_venv = data.is_ephemeral() && is_too_old;
    debug!("venv `{venv_id}` is too old: {is_too_old}");
    if should_remove_venv {
        debug!(
            "removing venv `{venv_id}` from the shared storage, as it's too old and is ephemeral"
        );
        dir.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                dir.path().display()
            )
        })?;
        removed_venvs.push(venv_id);
        return Ok(());
    }
    // We're done mutating data, let's make borrow checker happy.
    let data = venv.data();
    if requires_save {
        venv.save_to(storage)?;
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
