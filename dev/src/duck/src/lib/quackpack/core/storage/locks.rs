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
//! - [`RunLock`]: *pins* venv, such that dependencies can be safely loaded, while
//!   avoiding holding lock guarding venv data for too long. Holding this lock
//!   guarantees that dependencies saved in venv data will not change, even
//!   without holding venv data lock — operations which require mutable access to
//!   that file will wait for release of this lock (or fail without blocking).
//!   For temporary venvs, the [`RunLock`] is not sufficient, since clean operation
//!   may delete the venv. In that case to guarantee that dependencies will not change
//!   (be deleted), the data lock must be held.
//! - [`TrySyncLock`]: grants mutable access to the storage-stored virtual environment
//!   configuration. Respects all of the conditions given in the descriptions of the
//!   previous two locks. If the operation would block, [`WouldBlock`](io::ErrorKind::WouldBlock) is
//!   returned instead. As we do not assume any fair queueing of lock operations,
//!   this prevents error-prone situation, in which two concurrent synchronization
//!   operations would execute out of the order in which the user started them.
//!
//! Control over access to a virtual environment data file should be done using
//! [`PathExt::lock_shared`](rustvil::fs::PathExt::lock_shared)/[`PathExt::lock`](rustvil::fs::PathExt::lock)
//!
//! All implementations use the following locks:
//!
//! - CLEAN_LOCK: paths.clean_lock(storage)
//! - SYNC_LOCK[venv_id]: paths.venv_sync_lock(storage, venv_id)
//!
//! However, they are used differently on different platforms:
//!
//! - on posix and windows, both are shared-exclusive locks:
//!
//!   - holding [`CleanLock`] is exclusive access to CLEAN_LOCK,
//!   - holding [`TrySyncLock`] is shared access to CLEAN_LOCK and exclusive
//!     access to SYNC_LOCK[venv_id],
//!   - holding [`RunLock`] is shared access to SYNC_LOCK[venv_id]
//!
//! - on other platforms `SoftwareFileLock`
//!   is used: it works by exclusively creating the file to acquire and delete
//!   it to release. It only provides exclusive locks, so the locking
//!   mechanism is somewhat different:
//!
//!   - taking [`CleanLock`] takes CLEAN_LOCK and then waits for all SYNC_LOCK-s
//!     to be released, but only those that were present at the start of the
//!     operation (that is explained below in [`RunLock`] description),
//!   - taking [`TrySyncLock`] needs CLEAN_LOCK while acquiring SYNC_LOCK — that
//!     makes it so that if clean operation has started, no new synchronization
//!     can start (and clean operation waits for all ongoing ones),
//!   - [`RunLock`] takes SYNC_LOCK exclusively instead — that is less efficient
//!     that locking it in a shared mode, but we do not have access to that.
//!     We do not need to take CLEAN_LOCK as in synchronization operation,
//!     as run operation does not need mutable access to venv dependencies.
//!     The clean lock might wait for completion of all run operations as they too
//!     hold SYNC_LOCK, but as it takes a snapshot of the list of held locks,
//!     any newly spawned run operations do not delay clean operation any further.
//!
//!
//! Note that the `SoftwareFileLock` implementation is susceptible to deadlocks:
//! if process holding a lock exists abnormally, it does not delete the file
//! representing the held lock, which blocks all future operations which
//! attempt to lock it. Similarly, when system failure occurs, no locks
//! are freed.
//!
//! The posix / windows implementations avoid this issue by using locks provided
//! by operating system, which are freed when process is killed and vanish
//! on system reboot. Locks only provide exclusiveness of operations which
//! modify state, so freeing a lock of killed process does not have any negative
//! impact on state coherency.

use std::fs::ReadDir;
use std::io;

use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv_id::{ToVenvId, VenvId};
#[cfg(not(windows))]
use crate::util::filesystem::Filesystem;
use crate::util::filesystem::LockedFile;
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
    _compile_lock: LockedFile,
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
    pub fn new(storage: &Storage, venv_id: VenvId, ctx: &DuckContext) -> QuackResult<Option<Self>> {
        let clean_lock = storage.shared_clean_lock(ctx)?;
        let sync_lock = storage.sync_locks().try_open_shared_rw_create(venv_id)?;
        let Some(sync_lock) = sync_lock else {
            return Ok(None);
        };
        Ok(Some(Self {
            clean_lock,
            _sync_lock: sync_lock,
        }))
    }

    /// Upgrade self to a [`CompileLock`].
    pub fn to_compile_lock(
        self,
        storage: &Storage,
        venv_id: VenvId,
        ctx: &DuckContext,
    ) -> QuackResult<CompileLock> {
        let compile_lock = storage.compile_locks().open_exclusive(venv_id, ctx)?;
        Ok(CompileLock {
            _compile_lock: compile_lock,
            _clean_lock: self.clean_lock,
        })
    }
}

/// Cleans up leftover lock files from previously aborted or crashed processes.
/// Deletes lock files only if the corresponding virtual environment directories no longer exist.
pub fn cleanup_locks(storage: &Storage) -> QuackResult<()> {
    cleanup_locks_impl(storage, storage.iter_sync_locks()?, storage.sync_locks())?;
    cleanup_locks_impl(storage, storage.iter_data_locks()?, storage.data_locks())?;
    cleanup_locks_impl(
        storage,
        storage.iter_compile_locks()?,
        storage.compile_locks(),
    )?;
    Ok(())
}

/// An implementation detail of [`cleanup_locks`].
/// Removes all venv locks from the given iterator.
fn cleanup_locks_impl(
    storage: &Storage,
    dir_iterator: ReadDir,
    root: Filesystem,
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
fn try_delete_lock(root: &Filesystem, name: VenvId) -> QuackResult<()> {
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
fn try_delete_lock(root: &Filesystem, name: VenvId) -> QuackResult<()> {
    use crate::util::path_ops_ext::PathOpsExt;

    let guard = match root.try_open_exclusive(name) {
        Ok(guard) => guard,
        Err(e) => {
            let Some(err) = e.downcast_ref_in_chain::<io::Error>() else {
                return Err(e);
            };
            if err.kind() == io::ErrorKind::NotFound {
                return Ok(());
            };
            // We only try to delete lock if that is possible, if we would need
            // to block we skip that lock.
            if err.kind() == io::ErrorKind::WouldBlock
                || err.kind() == io::ErrorKind::PermissionDenied
            {
                None
            } else {
                return Err(e);
            }
        }
    };
    // Due to the lock taking mechanism we use, we may simply
    // delete the file if we own it, see `_posix_acquire`.
    if let Some(ref guard) = guard {
        guard.path().rm()?;
    };
    Ok(())
}
