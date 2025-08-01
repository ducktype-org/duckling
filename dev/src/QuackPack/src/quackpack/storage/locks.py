"""
Provides locks for accessing storage:

- ``CleanLock``: holding this lock implies that there are no running operations
  that require mutable access to the data of any virtual environment (by data we
  mean its resolved dependencies and other data copied from the user configuration
  file) — example of such operation is virtual environment synchronization.
  Any such future operations must wait for the release of the ``CleanLock``
  (or fail without blocking). ``CleanLock`` is used for operations that require
  mutable access to the storage as a whole: during clean operation we
  delete existing packages, so operations which could make a new reference
  to an orphaned package must be ruled out.
- ``RunLock``: "pins" venv, such that dependencies can be safely loaded, while
  avoiding holding lock guarding venv data for too long. Holding this lock
  guarantees that dependencies saved in venv data will not change, even
  without holding venv data lock — operations which require mutable access to
  that file will wait for release of this lock (or fail without blocking).
  For temporary venvs, the ``RunLock`` is not sufficient, since clean operation
  may delete the venv. In that case to guarantee that dependencies will not change
  (be deleted), the data lock must be held.
- ``TrySyncLock``: grants mutable access to the storage-stored virtual environment
  configuration. Respects all of the conditions given in the descriptions of the
  previous two locks. If the operation would block, `quackpack.util.locks.common.LockWouldBlock` is
  raised instead. As we do not assume any fair queueing of lock operations,
  this prevents error-prone situation, in which two concurrent synchronization
  operations would execute out of the order in which the user started them.

Control over access to a virtual environment data file should be done using
``quackpack.util.lock.FileLock``.

All implementations use the following locks:

- CLEAN_LOCK: paths.clean_lock(storage)
- SYNC_LOCK[venv_id]: paths.venv_sync_lock(storage, venv_id)

However, they are used differently on different platforms:

- on posix and windows, both are shared-exclusive locks:

  - holding ``CleanLock`` is exclusive access to CLEAN_LOCK,
  - holding ``TrySyncLock`` is shared access to CLEAN_LOCK and exclusive
    access to SYNC_LOCK[venv_id],
  - holding ``RunLock`` is shared access to SYNC_LOCK[venv_id]

- on other platforms ``quackpack.util.lock.SoftwareFileLock``
  is used: it works by exclusively creating the file to acquire and delete
  it to release. It only provides exclusive locks, so the locking
  mechanism is somewhat different:

  - taking ``CleanLock`` takes CLEAN_LOCK and then waits for all SYNC_LOCK-s
    to be released, but only those that were present at the start of the
    operation (that is explained below in ``RunLock`` description),
  - taking ``TrySyncLock`` needs CLEAN_LOCK while acquireing SYNC_LOCK — that
    makes it so that if clean operation has started, no new synchronization
    can start (and clean operation waits for all ongoing ones),
  - ``RunLock`` takes SYNC_LOCK exclusively instead — that is less efficient
    that locking it in a shared mode, but we do not have access to that.
    We do not need to take CLEAN_LOCK as in synchronization operation,
    as run operation does not need mutable access to venv dependencies.
    The clean lock might wait for completion of all run operations as they too
    hold SYNC_LOCK, but as it takes a snapshot of the list of held locks,
    any newly spawned run operations do not delay clean operation any further.

Note that the ``SoftwareFileLock`` implementation is susceptible to deadlocks:
if process holding a lock exists abnormally, it does not delete the file
representing the held lock, which blocks all future operations which
attempt to lock it. Similarily, when system failure occurs, no locks
are freed.

The posix / windows implementations avoid this issue by using locks provided
by operating system, which are freed when process is killed and vanish
on system reboot. Locks only provide exclusiveness of operations which
modify state, so freeing a lock of killed process does not have any negative
impact on state coherency.
"""

from types import TracebackType

from quackpack.signals import EnableInterrupt
from quackpack.storage.paths import StoragePaths
from quackpack.util.lock import FileLock
from quackpack.util.lock.common import LockType
from quackpack.util.types.pkgid import Identifier


class CleanLock:
    """
    A lock that guarantees no virtual environment data mutations are in progress.

    This lock ensures exclusive access to the global state of the storage. It is
    intended for operations like garbage collection or clean-up which may remove
    shared resources.

    On POSIX/Windows, acquires an exclusive lock on the clean lock file.
    On platforms lacking shared lock support, it also waits for all sync locks
    present at the time of acquisition to be released.

    :param storage: The storage layout containing lock file paths.
    :type storage: quackpack.storage.paths.StoragePaths
    """

    def __init__(self, storage: StoragePaths) -> None:
        self.storage: StoragePaths = storage
        self.lock = FileLock(self.storage.clean_lock(), lock_type=LockType.EXCLUSIVE, blocking=True)

    def __enter__(self) -> None:
        self.lock.__enter__()
        if FileLock.supports_shared():
            return
        try:
            # Convert the lock iterator into list to take a snaphot of held
            # locks at the start of this operation: any new locks taken
            # are not conflicting with CleanLock, otherwise holding the clean_lock
            # would prevent taking them.
            for venv in list(self.storage.iter_sync_locks()):
                # wait until lock is released
                with FileLock(venv):
                    pass
        except Exception:
            self.lock.__exit__(None, None, None)
            raise

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        return self.lock.__exit__(exc_type, exc_value, traceback)


class TrySyncLock:
    """
    A non-blocking lock for mutable access to a virtual environment's dependencies.

    Ensures the operation does not interfere with global clean operations or
    other concurrent synchronization tasks. If it cannot acquire the required
    locks, it raises `quackpack.util.locks.common.LockWouldBlock`.

    :param storage: The storage layout manager.
    :type storage: quackpack.storage.paths.StoragePaths
    :param venv_id: The identifier of the virtual environment.
    :type venv_id: quackpack.util.pkgid.Identifier
    """

    def __init__(self, storage: StoragePaths, venv_id: Identifier) -> None:
        self.storage: StoragePaths = storage
        self.venv_id: Identifier = venv_id
        self.clean_lock = FileLock(self.storage.clean_lock(), lock_type=LockType.SHARED, blocking=False)
        self.sync_lock = FileLock(
            self.storage.venv_sync_lock(self.venv_id), lock_type=LockType.EXCLUSIVE, blocking=False
        )

    def __enter__(self) -> None:
        if FileLock.supports_shared():
            self.clean_lock.__enter__()
            try:
                self.sync_lock.__enter__()
            except Exception:
                self.clean_lock.__exit__(None, None, None)
                raise
        else:
            with self.clean_lock:
                self.sync_lock.__enter__()

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        err: Exception | None = None
        try:
            self.sync_lock.__exit__(None, None, None)
        except Exception as close_err:
            err = close_err
        if FileLock.supports_shared():
            try:
                self.clean_lock.__exit__(None, None, None)
            except Exception as close_err:
                if err is not None:
                    raise ExceptionGroup("multiple cleanup close errors occured", [err, close_err]) from None
                else:
                    raise
        if err is not None:
            raise err
        if exc_type is not None:
            return False


class RunLock:
    """
    A lock granting safe read-only access to a virtual environment's dependency configuration.

    Holding this lock guarantees the dependencies remain unchanged for the duration.

    :param storage: The storage layout manager.
    :type storage: quackpack.storage.paths.StoragePaths
    :param venv_id: The identifier of the virtual environment.
    :type venv_id: quackpack.util.pkgid.Identifier
    """

    def __init__(self, storage: StoragePaths, venv_id: Identifier) -> None:
        self.lock = FileLock(storage.venv_sync_lock(venv_id), lock_type=LockType.SHARED, blocking=True)

    def __enter__(self) -> None:
        self.lock.__enter__()

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        return self.lock.__exit__(exc_type, exc_value, traceback)


def cleanup_locks(storage: StoragePaths) -> None:
    """
    Cleans up leftover lock files from previously aborted or crashed processes.

    Deletes lock files only if the corresponding virtual environment directories no longer exist.

    :param storage: The storage layout used to enumerate lock files and venv directories.
    :type storage: quackpack.storage.paths.StoragePaths
    """

    for lockfile in storage.iter_sync_locks():
        venv_id = Identifier(lockfile.name)
        with EnableInterrupt():
            pass
        if not storage.venv_dir(venv_id).is_dir():
            FileLock.try_delete_lock(lockfile)
    for lockfile in storage.iter_data_locks():
        venv_id = Identifier(lockfile.name)
        with EnableInterrupt():
            pass
        if not storage.venv_dir(venv_id).is_dir():
            FileLock.try_delete_lock(lockfile)
