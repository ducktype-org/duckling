from contextlib import suppress
from pathlib import Path
from time import sleep
from types import TracebackType
from typing import Final, override

from quackpack.signals import EnableInterrupt
from quackpack.util.lock.common import BaseFileLock, LockType, LockWouldBlock

PROBE_INTERVAL: Final[float] = 0.25


class SoftwareFileLock(BaseFileLock):
    @override
    def __init__(
        self, path: Path, lock_type: LockType = LockType.EXCLUSIVE, *, blocking: bool = True
    ) -> None:
        self.path = path
        self.lock_type = lock_type
        self.blocking = blocking

    @override
    @classmethod
    def supports_shared(cls) -> bool:
        return False

    @override
    @classmethod
    def try_delete_lock(cls, path: Path) -> None:
        # Locks are deleted during release.
        pass

    @override
    def __enter__(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        while True:
            with suppress(FileExistsError):
                # We use zero mode, as that does not prevent deleting the
                # file programmatically (that only needs write permissions
                # to directory), but may be checked by external tools, such
                # as `rm` command (it might ask when trying to remove
                # write-protected file).
                self.path.touch(mode=0, exist_ok=False)
                break
            if not self.blocking:
                raise LockWouldBlock from None
            with EnableInterrupt():
                sleep(PROBE_INTERVAL)

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        self.path.unlink()
        return None if exc_type is None else False
