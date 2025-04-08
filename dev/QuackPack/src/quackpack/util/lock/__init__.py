"""
Provides file based inter-process locking mechanism.
When possible, the locks are implemented using operating system interfaces, only
falling back to in-software implementation (where existence of a file is a marker
of holding a lock) when current operating system is not supported.

Usage of the provided file lock is valid only on the main thread and only when
the robust signal handler is enabled. Getting signal interrupt while waiting
on a file lock results in raising a `SignalInterrupt` exception.

The implementation does not need to support shared locks, to check if it does,
use `FileLock.supports_shared`. Notably in-software implementation does not
support those.

Nothing is guaranteed in regard to reentrancy of provided locks.

Holding the same lock concurrently in the same process is invalid (this is mainly
motivated by posix, where closing ANY file descriptor that refers to locked file,
removes ALL locks corresponding to that file, owned by current process).
"""

import os
import sys

from quackpack.util.lock.common import BaseFileLock, Blocking, LockType, LockWouldBlock

FileLock: type[BaseFileLock]
if os.name == "posix":
    from quackpack.util.lock.posix import PosixFileLock

    FileLock = PosixFileLock
elif sys.platform == "win32":
    from quackpack.util.lock.windows import WindowsFileLock

    FileLock = WindowsFileLock
else:
    from quackpack.util.lock.software import SoftwareFileLock

    FileLock = SoftwareFileLock

__all__ = ["BaseFileLock", "Blocking", "FileLock", "LockType", "LockWouldBlock"]
