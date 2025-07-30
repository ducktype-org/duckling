import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Never, override

from pydantic import BaseModel

from quackpack.compile.code_sink import CodeSink, CodeSinkConnection, Continuation
from quackpack.storage.files import VenvFreeze
from quackpack.util.types.pkgid import PackageId


@dataclass(frozen=True, kw_only=True)
class CompilerArgs:
    pass


class ExitContinuation(Continuation):
    @override
    def execute(self) -> Never:
        sys.exit()


class ExecContinuation(Continuation):
    def __init__(self, path: Path | str, args: list[str]):
        self.path = path
        self.args = args

    @override
    def execute(self) -> Never:
        os.execvp(self.path, self.args)


class Compile(CodeSink):
    def __init__(self, duckc_exec: str, args: CompilerArgs):
        self.args = args
        self.duckc_exec = duckc_exec

    @override
    def connect(self) -> CodeSinkConnection:
        print(f"CONNECTING TO COMPILER: {self.args}")
        return CompileConnection(self.duckc_exec)


# NOTE: this is temporary implementation until the compiler daemon is implemented.
class CompileConnection(CodeSinkConnection):
    def __init__(self, duckc_exec: str):
        self.duckc_exec = duckc_exec
        self.artifacts: str | None = None
        self.sources: str | None = None
        self.dependencies: str | None = None

    @override
    def load_dependencies(self, freeze: VenvFreeze, path_mapping: dict[PackageId, Path]) -> None:
        # print(f"LOAD_DEPENDENCIES: venv freeze: {freeze}, paths: {path_mapping}")

        class _DependencyCmdEntry(BaseModel):
            freeze: VenvFreeze
            path_mapping: list[tuple[PackageId, str]]

        path_mappings = [(k, str(v)) for k, v in path_mapping.items()]

        # NOTE: This format is temporary.
        self.dependencies = _DependencyCmdEntry(freeze=freeze, path_mapping=path_mappings).model_dump_json()

    def get_compiler_args(self) -> list[str]:
        assert self.artifacts is not None
        assert self.sources is not None
        assert self.dependencies is not None
        return ["-a", self.artifacts, "-m", self.sources, "-d", self.dependencies]

    @override
    def finalize(self, entry_path: Path) -> Continuation:
        # print(f"COMPILING: entry_path {entry_path}")

        # TODO this is wrong
        self.artifacts = str(entry_path / ".build")

        self.sources = str(entry_path)

        assert self.dependencies is not None
        return ExecContinuation(self.duckc_exec, self.get_compiler_args())


# NOTE: this is temporary implementation until the compiler daemon is implemented.
class CompileAndRun(CodeSink):
    def __init__(self, duckc_exec: str, args: CompilerArgs, run_args: list[str]):
        self.args = args
        self.run_args = run_args
        self.duckc_exec = duckc_exec

    @override
    def connect(self) -> CodeSinkConnection:
        print(f"CONNECTING TO COMPILER: {self.args}")
        return CompileAndRunConnection(self.duckc_exec, self.run_args)


class CompileAndRunConnection(CompileConnection):
    def __init__(self, duckc_exec: str, run_args: list[str]):
        super().__init__(duckc_exec)
        self.run_args = run_args

    @override
    def get_compiler_args(self) -> list[str]:
        return [*super().get_compiler_args(), "--run", *self.run_args]
