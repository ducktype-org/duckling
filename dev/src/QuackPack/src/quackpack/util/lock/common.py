from abc import ABC, abstractmethod
from contextlib import AbstractContextManager
from enum import Enum, auto
from pathlib import Path


class LockType(Enum):
    EXCLUSIVE = auto()
    SHARED = auto()


class LockWouldBlock(Exception):
    pass


class BaseFileLock(AbstractContextManager[None], ABC):
    """
    Common interface of lock file implementations. Used as a context manager.
    Usage is valid only on the main thread and only when robust signal handler
    is enabled. If signal interrupt happens while waiting on the lock,
    `SignalInterrupt` exception is raised.
    """

    @abstractmethod
    def __init__(
        self, path: Path, lock_type: LockType = LockType.EXCLUSIVE, *, blocking: bool = True
    ) -> None: ...

    @classmethod
    @abstractmethod
    def supports_shared(cls) -> bool:
        """
        Returns if the lock implementation supports shared locks.
        If this is false, all locks with LockType.SHARED will behave
        as exclusive locks.
        """
        ...

    @classmethod
    @abstractmethod
    def try_delete_lock(cls, path: Path) -> None:
        """
        Tries to delete the lock pointed to by `path` argument.
        If this function would block, it skips deleting the lock instead.
        """
        ...
