import errno
import os
from contextlib import nullcontext, suppress
from fcntl import LOCK_EX, LOCK_NB, LOCK_SH, lockf
from pathlib import Path
from types import TracebackType
from typing import override

from quackpack.core.signals import EnableInterrupt
from quackpack.util.context_managers import OsFdContext, UMaskContext
from quackpack.util.lock.common import BaseFileLock, LockType, LockWouldBlock


def _posix_acquire(path: Path, lock_type: int) -> int:
    """
    Acquires the lock in a way, that gives a possibility of safely deleting it.

    The mechanism works as follows:
    We assume process may only delete a lockfile, while holding an exclusive
    lock on it. When `lockf` call from the loop has finished we hold a lock,
    to either the current lock file or unlinked previous version.
    To differentiate those cases we check if the inode number from
    out opened lock and current filesystem file agree.
    Also note: the ABA problem cannot occur, as unlinked opened file
    still has a valid inode that cannot be reused until all references
    to it are dropped.

    Source:
    https://stackoverflow.com/a/18745264

    Returns: file descriptor of acquired lock

    Args:
        path: path to the lockfile
        lock_type: flags passed to `lockf`: LOCK_SH, LOCK_EX, LOCK_NB
    """
    # When creating the lock file, we try to use the same (rw) permissions as the
    # parent directory. This allows sharing locks across group (or even globally)
    # if parent directory is marked as such.
    permissions = os.stat(path.parent).st_mode & 0o666
    while True:
        fd: int | None = None
        try:
            with UMaskContext(0):
                fd = os.open(path, mode=permissions, flags=os.O_RDWR | os.O_CREAT)
            # We use nested try, as other operating system operations may also raise
            # exceptions with EACCES or EAGAIN code.
            try:
                with EnableInterrupt() if (lock_type & LOCK_NB) == 0 else nullcontext():
                    lockf(fd, lock_type)
            except OSError as err:
                # Posix does not define which of those two errors are returned
                # when trying to acquire held lock with LOCK_NB.
                if err.errno in (errno.EACCES, errno.EAGAIN):
                    raise LockWouldBlock from err
                raise

            obtained_stat = os.fstat(fd)
            # If FileNotFoundError occurs, the lock we obtained no longer guarantees
            # locking across processes, so we retry the locking process.
            with suppress(FileNotFoundError):
                current_stat = os.stat(path)
                if obtained_stat.st_ino == current_stat.st_ino:
                    return fd

        except Exception:
            if fd is not None:
                # Suppress the close error to not add it to backtrace.
                with suppress(OSError):
                    os.close(fd)
            raise
        else:  # If we did not error and did not return, we reset the fd and try again.
            os.close(fd)


class PosixFileLock(BaseFileLock):
    @override
    def __init__(
        self,
        path: Path,
        lock_type: LockType = LockType.EXCLUSIVE,
        *,
        blocking: bool = True
    ) -> None:
        self.path: Path = path
        self.flags: int = (LOCK_EX if lock_type is LockType.EXCLUSIVE else LOCK_SH) | (
            LOCK_NB if not blocking else 0
        )
        self.fd: int | None = None

    @override
    @classmethod
    def supports_shared(cls) -> bool:
        return True

    @override
    @classmethod
    def try_delete_lock(cls, path: Path) -> None:
        with suppress(FileNotFoundError), OsFdContext(path, os.O_RDWR) as fd:
            try:
                lockf(fd, LOCK_EX | LOCK_NB)
            except OSError as err:
                # We only try to delete lock if that is possible, if we would need
                # to block we skip that lock.
                if err.errno not in (errno.EACCES, errno.EAGAIN):
                    raise
            else:
                # Due to the lock taking mechanism we use, we may simply
                # delete the file if we own it, see `_posix_acquire`.
                path.unlink()

    @override
    def __enter__(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.fd = _posix_acquire(self.path, self.flags)

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        if self.fd is not None:
            os.close(self.fd)
        return None if exc_type is None else False
