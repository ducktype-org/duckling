from dataclasses import dataclass
from pathlib import Path

from quackpack.compile import CompileAndRun, CompilerArgs
from quackpack.global_context import GlobalContext
from quackpack.package import Package
from quackpack.storage.storage import run as run_impl
from quackpack.util.types.errors import QuackPackError


@dataclass(frozen=True, kw_only=True)
class RunOptions:
    ctx: GlobalContext

    package: Package
    """
    Package to be compiled and run.
    """

    compiler_args: CompilerArgs

    exec_args: list[str]

    entry_path: Path | None

    duckc_binary: str


def run(opts: RunOptions):
    entry_path = opts.entry_path
    if entry_path is None:
        if opts.package.is_global:
            raise QuackPackError("Cannot run global virtual environment")
        else:
            entry_path = opts.package.package_root / "src" / "main.duck"
    runner = CompileAndRun(opts.duckc_binary, opts.compiler_args, opts.exec_args)
    continuation = run_impl(opts.ctx, opts.package, runner, entry_path.expanduser().resolve())
    # Note: we instantly end the program here.
    # If there are changes that add something important up the stacktrace,
    # we may need to return the Continuation and pass it upwards.
    continuation.execute()
