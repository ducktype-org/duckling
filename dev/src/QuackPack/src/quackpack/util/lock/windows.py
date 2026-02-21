from contextlib import nullcontext, suppress
from pathlib import Path
from types import TracebackType
from typing import TYPE_CHECKING, Any, Literal, override

import pywintypes
import win32api
import win32file
from win32con import LOCKFILE_EXCLUSIVE_LOCK, LOCKFILE_FAIL_IMMEDIATELY
from win32file import (
    FILE_ATTRIBUTE_NORMAL,
    FILE_SHARE_READ,
    FILE_SHARE_WRITE,
    GENERIC_READ,
    GENERIC_WRITE,
    OPEN_ALWAYS,
    CreateFile,
    LockFileEx,  # type: ignore[attr-defined]
)

from quackpack.core.signals import EnableInterrupt
from quackpack.util.lock.common import BaseFileLock, LockType, LockWouldBlock

if TYPE_CHECKING:
    from _win32typing import PyHANDLE, PyOVERLAPPED
else:  # Fallback for runtime
    PyHANDLE = Any
    PyOVERLAPPED = Any


LOCKFILE_SHARED_LOCK: Literal[0] = 0


ERROR_FILE_NOT_FOUND: Literal[0x2] = 0x2
ERROR_SHARING_VIOLATION: Literal[0x20] = 0x20
ERROR_LOCK_VIOLATION: Literal[0x21] = 0x21


class WindowsFileLock(BaseFileLock):
    @override
    def __init__(
        self,
        path: Path,
        lock_type: LockType = LockType.EXCLUSIVE,
        *,
        blocking: bool = True
    ) -> None:
        self.path: Path = path
        self.flags: int = (
            LOCKFILE_EXCLUSIVE_LOCK
            if lock_type is LockType.EXCLUSIVE
            else LOCKFILE_SHARED_LOCK
        ) | (LOCKFILE_FAIL_IMMEDIATELY if not blocking else 0)
        self.fd: PyHANDLE | None = None
        self.ov: PyOVERLAPPED | None = None

    @override
    @classmethod
    def supports_shared(cls) -> bool:
        return True

    @override
    @classmethod
    def try_delete_lock(cls, path: Path) -> None:
        # On Windows, trying to delete file opened by another process
        # leads to an ERROR_SHARING_VIOLATION error.
        try:
            win32file.DeleteFile(str(path))
        except win32api.error as err:
            if err.winerror not in (ERROR_SHARING_VIOLATION, ERROR_FILE_NOT_FOUND):
                raise

    @override
    def __enter__(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self.fd = CreateFile(
                str(self.path),
                # LockFileEx requires GenericWrite
                GENERIC_READ | GENERIC_WRITE,
                # This is another mechanism for locking, which however does
                # not support waiting. We thus allow sharing as lock accesses
                # must open the same file.
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None,  # security attributes
                OPEN_ALWAYS,  # create if file does not exist
                FILE_ATTRIBUTE_NORMAL,
                None,  # template file
            )
            # OVERLAPPED is a Windows structure used in context of asynchronous
            # IO operations. When constructing with `pywintypes.OVERLAPPED()`,
            # it specifies blocking operation, but it could be configured with
            # a waitable asynchronous event (we do not need that here).
            # It also provides file position of beginning of the locked file
            # fragment. The length of the fragment is provided by the two
            # constants, they are 32-bit ints which combine to form 64-bit value.
            # In this usage that is `(0xFFFF0000 << 32) | 0`: the fragment is
            # made very long to make sure there will not be multiple locks on
            # the file (we probably do not need that, but it is good practise
            # if the code will be reused somewhere else).
            self.ov = pywintypes.OVERLAPPED()
            with (
                EnableInterrupt()
                if (self.flags & LOCKFILE_FAIL_IMMEDIATELY) == 0
                else nullcontext()
            ):
                LockFileEx(self.fd.handle, self.flags, 0, 0xFFFF0000, self.ov)
        except Exception as err:
            if self.fd is not None:
                # Suppress the close error to not add it to backtrace.
                with suppress(win32api.error):
                    self.fd.close()
            if isinstance(err, win32api.error) and err.winerror == ERROR_LOCK_VIOLATION:
                raise LockWouldBlock from err
            raise

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        if self.fd is not None:
            self.fd.close()
        return None if exc_type is None else False
