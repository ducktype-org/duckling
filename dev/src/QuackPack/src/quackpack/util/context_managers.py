import os
from contextlib import AbstractContextManager
from pathlib import Path
from types import TracebackType
from typing import override


class OsFdContext(AbstractContextManager[int]):
    """
    Context for opening files using `os` module and closing them after exiting
    the context. As `os.open` returns ordinary integers, `contextlib.closing`
    does not apply in that situation.
    """

    def __init__(self, path: Path, flags: int, dir_fd: int | None = None) -> None:
        self.path = path
        self.dir_fd = dir_fd
        self.flags = flags
        self.fd = None

    @override
    def __enter__(self) -> int:
        self.fd = os.open(self.path, flags=self.flags, dir_fd=self.dir_fd)
        return self.fd

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


class UMaskContext(AbstractContextManager[None]):
    """
    Context for temporarily changing process umask.
    """

    def __init__(self, umask: int) -> None:
        self.umask = umask

    @override
    def __enter__(self) -> None:
        self.umask = os.umask(self.umask)

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        os.umask(self.umask)
        return None if exc_type is None else False
