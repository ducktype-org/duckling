//! Provides locks for accessing storage:
//!
//! - [`CleanLock`]: holding this lock implies that there are no running operations
//!   that require mutable access to the data of any virtual environment (by data we
//!   mean its resolved dependencies and other data copied from the user configuration
//!   file) — example of such operation is virtual environment synchronization.
//!   Any such future operations must wait for the release of the [`CleanLock`]
//!   (or fail without blocking). [`CleanLock`] is used for operations that require
//!   mutable access to the storage as a whole: during clean operation we
//!   delete existing packages, so operations which could make a new reference
//!   to an orphaned package must be ruled out.
//! - [`TrySyncLock`]: grants mutable access to the storage-stored virtual environment
//!   configuration. Respects all of the conditions given in the descriptions of the
//!   previous two locks. If the operation would block, [`None`] is
//!   returned instead. As we do not assume any fair queueing of lock operations,
//!   this prevents error-prone situation, in which two concurrent synchronization
//!   operations would execute out of the order in which the user started them.
//! - [`CompileLock`]: holding this lock guarantees, that no CLEAN operation is in progress.
//!   Note that this lock does not BLOCK synchronizing this venv afterwards.
//!
//! All implementations use the following locks:
//! - holding the [`CleanLock`] is an exclusive access to the CLEAN_LOCK,
//! - holding the [`TrySyncLock`] is a shared access to the CLEAN_LOCK and an exclusive
//!   access to SYNC_LOCK[venv_id],
//! - the [`CompileLock`] can be created from the [`TrySyncLock`] by dismissing the SYNC_LOCK[venv_id],
//!   and keeping only a shared CLEAN_LOCK.

use std::fs::ReadDir;
use std::io;

use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
#[cfg(not(windows))]
use crate::util::file_locks::FileLockManager;
use crate::util::file_locks::LockedFile;
use crate::{DuckContext, QuackResult, QuackResultContext};

#[derive(Debug)]
/// A lock that guarantees no virtual environment data mutations are in progress.
///
/// This lock ensures exclusive access to the global state of the storage. It is
/// intended for operations like garbage collection or clean-up which may remove
/// shared resources.
///
/// # Original comment (no longer relevant, all locks support shared)
/// On POSIX/Windows, acquires an exclusive lock on the clean lock file.
/// On platforms lacking shared lock support, it also waits for all sync locks
/// present at the time of acquisition to be released.
pub struct CleanLock {
    _lock: LockedFile,
}

impl CleanLock {
    /// Create a new [`CleanLock`] for the given storage.
    pub fn new(storage: &Storage, ctx: &DuckContext) -> QuackResult<Self> {
        let lock = storage.exclusive_clean_lock(ctx)?;
        Ok(Self { _lock: lock })
    }
}

#[derive(Debug)]
/// Counterpart to [`CleanLock`], which blocks latter from being acquired.
/// In practice this is shared form of [`CleanLock`].
///
/// Can be constructed from [`TrySyncLock::to_compile_lock`].
pub struct CompileLock {
    _clean_lock: LockedFile,
}

#[derive(Debug)]
/// A non-blocking lock for mutable access to a virtual environment's dependencies.
///
/// Ensures the operation does not interfere with global clean operations or
/// other concurrent synchronization tasks. If it cannot acquire the required
/// locks, it returns [`WouldBlock`](io::ErrorKind::WouldBlock).
pub struct TrySyncLock {
    clean_lock: LockedFile,
    _sync_lock: LockedFile,
}

impl TrySyncLock {
    /// Create a new [`TrySyncLock`] for the given venv in the given storage.
    /// Returns `Ok(None)`, if locking would block.
    pub fn new(storage: &Storage, venv_id: VenvId) -> QuackResult<Option<Self>> {
        let clean_lock = storage.shared_clean_lock()?;
        let Some(clean_lock) = clean_lock else {
            return Ok(None);
        };
        let sync_lock = storage.sync_locks().try_open_exclusive(venv_id)?;
        let Some(sync_lock) = sync_lock else {
            return Ok(None);
        };
        Ok(Some(Self {
            clean_lock,
            _sync_lock: sync_lock,
        }))
    }

    /// Upgrade self to a [`CompileLock`].
    pub fn into_compile_lock(self) -> CompileLock {
        CompileLock {
            _clean_lock: self.clean_lock,
        }
    }
}

/// Cleans up leftover lock files from previously aborted or crashed processes.
/// Deletes lock files only if the corresponding virtual environment directories no longer exist.
pub fn cleanup_locks(storage: &Storage) -> QuackResult<()> {
    cleanup_locks_impl(storage, storage.iter_sync_locks()?, storage.sync_locks())?;
    cleanup_locks_impl(storage, storage.iter_data_locks()?, storage.data_locks())?;
    Ok(())
}

/// An implementation detail of [`cleanup_locks`].
/// Removes all venv locks from the given iterator.
fn cleanup_locks_impl(
    storage: &Storage,
    dir_iterator: ReadDir,
    root: FileLockManager,
) -> QuackResult<()> {
    for lockfile in dir_iterator {
        let lockfile = lockfile.context("failed to read entry from dir iterator")?;
        let name = lockfile.file_name().to_venv_id();
        let path = lockfile.path();
        if !storage.venv_dir(name).is_dir() {
            try_delete_lock(&root, name)
                .with_context(|| format!("failed to delete lock `{}`", path.display()))?;
        }
    }
    Ok(())
}

/// On Windows, trying to delete file opened by another process
/// leads to an ERROR_SHARING_VIOLATION error.
#[cfg(windows)]
fn try_delete_lock(root: &FileLockManager, name: VenvId) -> QuackResult<()> {
    let path = root.not_locked_path().join(name);
    match path.not_locked_path().rm() {
        Ok(()) => Ok(()),
        Err(e)
            if e.kind() == io::ErrorKind::NotFound
                // ERROR_SHARING_VIOLATION is presumably mapped to PermissionDenied.
                || e.kind() == io::ErrorKind::PermissionDenied =>
        {
            Ok(())
        }
        Err(e) => Err(e.into()),
    }
}

#[cfg(not(windows))]
fn try_delete_lock(root: &FileLockManager, name: VenvId) -> QuackResult<()> {
    use crate::util::path_ops_ext::PathOpsExt;

    let guard = match root.try_open_exclusive(name) {
        Ok(guard) => guard,
        Err(e) => {
            let Some(err) = e.downcast_ref_in_chain::<io::Error>() else {
                return Err(e);
            };

            let kind = err.kind();
            // This should almost never happen, as we'll try to create a file in this case,
            // but we may hit EPERM nevertheless.
            if kind == io::ErrorKind::NotFound {
                return Ok(());
            // We only try to delete lock, if that is possible.
            } else if kind == io::ErrorKind::PermissionDenied {
                None
            } else {
                return Err(e);
            }
        }
    };
    if let Some(ref guard) = guard {
        guard.path().rm()?;
    };
    Ok(())
}
