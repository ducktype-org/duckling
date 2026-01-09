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
//! - [`RunLock`]: "pins" venv, such that dependencies can be safely loaded, while
//!   avoiding holding lock guarding venv data for too long. Holding this lock
//!   guarantees that dependencies saved in venv data will not change, even
//!   without holding venv data lock — operations which require mutable access to
//!   that file will wait for release of this lock (or fail without blocking).
//!   For temporary venvs, the [`RunLock`] is not sufficient, since clean operation
//!   may delete the venv. In that case to guarantee that dependencies will not change
//!   (be deleted), the data lock must be held.
//! - [`TrySyncLock`]: grants mutable access to the storage-stored virtual environment
//!   configuration. Respects all of the conditions given in the descriptions of the
//!   previous two locks. If the operation would block, `quackpack.util.locks.common.LockWouldBlock` is
//!   raised instead. As we do not assume any fair queueing of lock operations,
//!   this prevents error-prone situation, in which two concurrent synchronization
//!   operations would execute out of the order in which the user started them.
//!
//! Control over access to a virtual environment data file should be done using
//! [`PathExt::lock_shared`]/[`PathExt::lock`]
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
//! - on other platforms ``quackpack.util.lock.SoftwareFileLock``
//!   is used: it works by exclusively creating the file to acquire and delete
//!   it to release. It only provides exclusive locks, so the locking
//!   mechanism is somewhat different:
//!
//!   - taking ``CleanLock`` takes CLEAN_LOCK and then waits for all SYNC_LOCK-s
//!     to be released, but only those that were present at the start of the
//!     operation (that is explained below in ``RunLock`` description),
//!   - taking ``TrySyncLock`` needs CLEAN_LOCK while acquireing SYNC_LOCK — that
//!     makes it so that if clean operation has started, no new synchronization
//!     can start (and clean operation waits for all ongoing ones),
//!   - ``RunLock`` takes SYNC_LOCK exclusively instead — that is less efficient
//!     that locking it in a shared mode, but we do not have access to that.
//!     We do not need to take CLEAN_LOCK as in synchronization operation,
//!     as run operation does not need mutable access to venv dependencies.
//!     The clean lock might wait for completion of all run operations as they too
//!     hold SYNC_LOCK, but as it takes a snapshot of the list of held locks,
//!     any newly spawned run operations do not delay clean operation any further.
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

use rustvil::fs::PathExt;
