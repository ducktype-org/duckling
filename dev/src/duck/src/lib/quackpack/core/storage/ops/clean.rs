// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Removing files from a storage.
use std::collections::HashSet;
use std::fs::DirEntry;
use std::path::{Path, PathBuf};
use std::time::Duration;

use chrono::{DateTime, Utc};
use storage::paths::Storage;
use storage::{locks, paths};
use tracing::{debug, error, info, warn};

use crate::quackpack::core::storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
use crate::util::error::ErrorsLogger;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug)]
/// An output of a [`clean_storage`].
pub struct CleanOutput {
    /// Ids of removed venvs.
    pub removed_venvs: Vec<VenvId>,
    /// Paths to the removed packages.
    pub removed_packages: Vec<PathBuf>,
    /// Errors encountered during this clean operation.
    pub encountered_errors: ErrorsLogger,
}

/// Delete a virtual environment from storage.
#[tracing::instrument(skip_all, fields(root = %storage_root.display(), id = %venv.to_venv_id()))]
pub fn delete_venv(ctx: &DuckContext, storage_root: &Path, venv: impl ToVenvId) -> QuackResult<()> {
    let storage = paths::Storage::new(storage_root);
    let venv_id = venv.to_venv_id();
    debug!("deleting venv");

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
    info!("deleted venv");
    ctx.info(format!("successfully removed venv `{venv_id}`"))?;
    Ok(())
}

/// Remove orphaned packages and expired temporary virtual environments from storage.
#[tracing::instrument(skip_all, fields(root = %storage_root.display()))]
pub fn clean_storage(ctx: &DuckContext, storage_root: &Path) -> QuackResult<CleanOutput> {
    debug!(root = %storage_root.display(), "cleaning storage");
    let temporary_lifetime = ctx.duck_cfg().storage_tmp_lifetime()?;
    let storage = paths::Storage::new(storage_root);
    let mut removed_venvs = vec![];
    let _lock = locks::CleanLock::new(&storage, ctx).context("failed to acquire a clean lock")?;
    let mut all_deps = HashSet::new();
    let now = Utc::now();
    let venvs = {
        let venvs = storage.iter_venvs()?;
        venvs.into_iter().collect::<Result<Vec<_>, _>>()?
    };
    debug!(?venvs, "removing venvs");
    let mut logger = ErrorsLogger::default();
    for venv in venvs {
        let _ = logger.log_result(clean_venv_from_storage(
            venv,
            &storage,
            temporary_lifetime,
            now,
            &mut removed_venvs,
            &mut all_deps,
            ctx,
        ));
    }
    info!("removed venvs");
    let _ =
        logger.log_result(locks::cleanup_locks(&storage).context("failed to cleanup some locks"));
    debug!(?all_deps, "used dependencies");
    let all_pkgs = logger
        .log_result(storage.iter_pkgs())
        .unwrap_or(storage::DirContents::Empty);
    let all_pkgs = logger
        .log_result(
            all_pkgs
                .map(|entry| entry.map_err(QuackError::from))
                .collect::<Result<Vec<_>, _>>(),
        )
        .unwrap_or_default();
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
        let _ = logger.log_result(pkg.rmtree());
    }
    info!(?packages_to_remove, "cleaned packages");
    ctx.info("successfully cleaned the storage")?;
    Ok(CleanOutput {
        removed_venvs,
        removed_packages: packages_to_remove,
        encountered_errors: logger,
    })
}

/// Remove a single venv from a storage.
/// A helper for [`clean_storage`].
#[tracing::instrument(skip_all, fields(?temporary_lifetime, %now, id = %dir.file_name().to_venv_id()))]
fn clean_venv_from_storage(
    dir: DirEntry,
    storage: &Storage,
    temporary_lifetime: Duration,
    now: DateTime<Utc>,
    removed_venvs: &mut Vec<VenvId>,
    all_deps: &mut HashSet<String>,
    ctx: &DuckContext,
) -> QuackResult<()> {
    let venv_id = dir.file_name().to_venv_id();
    debug!("removing venv");
    let venv = Venv::fix_and_load(storage, venv_id, ctx)?;
    let Some(mut venv) = venv else {
        error!("failed to fix and load venv");
        warn!("will clean regardless of what");
        dir.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                dir.path().display()
            )
        })?;
        info!("removed venv");
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
        debug!("requires_save, because it's too old");
    }
    let is_too_old = data.last_synchronization() + temporary_lifetime < now;
    let should_remove_venv = data.is_ephemeral() && is_too_old;
    debug!(
        ephemeral = %data.is_ephemeral(),
        last_synchronization = %data.last_synchronization(),
        %now,
        %is_too_old,
        %should_remove_venv,
    );
    if should_remove_venv {
        debug!("removing venv from the shared storage, as it's too old and is ephemeral");
        dir.path().rmtree().with_context(|| {
            format!(
                "while removing venv `{venv_id}` at `{}`",
                dir.path().display()
            )
        })?;
        info!("removed venv");
        removed_venvs.push(venv_id);
        return Ok(());
    }
    // We're done mutating data, let's make borrow checker happy.
    let data = venv.data();
    if requires_save {
        debug!("saving venv");
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
