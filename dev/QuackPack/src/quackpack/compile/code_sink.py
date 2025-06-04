from abc import ABC, abstractmethod
from pathlib import Path
from typing import Never

from quackpack.util.pkgid import Identifier, ResolvedId


class Continuation(ABC):
    @abstractmethod
    def execute(self) -> Never: ...


class CodeSinkConnection(ABC):
    @abstractmethod
    def load_library(self, _alias: Identifier, _dep: ResolvedId, _path: Path) -> None:
        """
        Asks the sink to load specified library. Blocks until the operation is completed.
        """

    @abstractmethod
    def check_platform(self, required_system: list[str] | None, required_arch: list[str] | None) -> bool:
        """
        Returns if the provided sytem requirements are satisfied.
        """

    @abstractmethod
    def finalize(self, entry_path: Path) -> Continuation:
        """
        Finalize the task and end the connection. Returns `Continuation` which is a callback
        used at the end of the program.
        """


class CodeSink(ABC):
    """
    An interface for `run` command endpoints: for example most notably compilation.
    """

    @abstractmethod
    def connect(self) -> CodeSinkConnection:
        """
        Initializes connection to the given code sink. Only use once.
        """
