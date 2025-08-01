from abc import ABC, abstractmethod
from pathlib import Path
from typing import Never

from quackpack.storage.files import VenvFreeze
from quackpack.util.types.pkgid import PackageId


class Continuation(ABC):
    @abstractmethod
    def execute(self) -> Never: ...


class CodeSinkConnection(ABC):
    @abstractmethod
    def load_dependencies(self, freeze: VenvFreeze, path_mapping: dict[PackageId, Path]) -> None:
        """
        Asks the sink to load the dependencies. Blocks until the operation is completed.
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
