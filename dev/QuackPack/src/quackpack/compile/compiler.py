import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Never, override

from quackpack.compile.code_sink import CodeSink, CodeSinkConnection, Continuation
from quackpack.util.pkgid import Identifier, ResolvedId


@dataclass(frozen=True, kw_only=True)
class CompilerArgs:
    pass


class ExitContinuation(Continuation):
    @override
    def execute(self) -> Never:
        sys.exit()


class ExecContinuation(Continuation):
    def __init__(self, path: Path, args: list[str]):
        self.path = path
        self.args = args

    @override
    def execute(self) -> Never:
        os.execv(self.path, self.args)


class Compile(CodeSink):
    def __init__(self, args: CompilerArgs):
        self.args = args

    @override
    def connect(self) -> CodeSinkConnection:
        print(f"CONNECTING TO COMPILER: {self.args}")
        return CompileConnection()


class CompileConnection(CodeSinkConnection):
    @override
    def load_library(self, alias: Identifier, dep: ResolvedId, path: Path) -> None:
        print(f"LOAD_LIBRARY: alias {alias}, dep: {dep}, path: {path}")

    @override
    def check_platform(self, required_system: list[str] | None, required_arch: list[str] | None) -> bool:
        return True

    @override
    def finalize(self, entry_path: Path) -> Continuation:
        print(f"COMPILING: entry_path {entry_path}")
        return ExitContinuation()


class CompileAndRun(CodeSink):
    def __init__(self, args: CompilerArgs, run_args: list[str]):
        self.args = args
        self.run_args = run_args

    @override
    def connect(self) -> CodeSinkConnection:
        print(f"CONNECTING TO COMPILER: {self.args}")
        return CompileAndRunConnection(self.run_args)


class CompileAndRunConnection(CompileConnection):
    def __init__(self, run_args: list[str]):
        self.run_args = run_args

    @override
    def finalize(self, entry_path: Path) -> Continuation:
        print(f"COMPILING: entry_path {entry_path}")
        # TODO get exec path from compiler
        return ExecContinuation(Path("/usr/bin/echo"), ["EXECUTING: ", *self.run_args])
