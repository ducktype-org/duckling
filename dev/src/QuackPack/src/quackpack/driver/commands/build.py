from dataclasses import dataclass
from pathlib import Path

from quackpack.compile.compiler import Compile, CompilerArgs
from quackpack.core.storage.storage import run as run_impl
from quackpack.core.types.package import Package
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.errors import QuackPackError


@dataclass(frozen=True, kw_only=True)
class BuildOptions:
    ctx: GlobalContext

    package: Package
    """
    Package to be compiled.
    """

    compiler_args: CompilerArgs

    entry_path: Path | None

    duckc_binary: str


def build(opts: BuildOptions):
    entry_path = opts.entry_path
    if entry_path is None:
        if opts.package.is_global:
            raise QuackPackError("Cannot build global virtual environment")
        else:
            entry_path = opts.package.package_root / "src" / "main.duck"
    runner = Compile(opts.duckc_binary, opts.compiler_args)
    continuation = run_impl(opts.ctx, opts.package, runner, entry_path.expanduser().resolve())
    # Note: we instantly end the program here.
    # If there are changes that add something important up the stacktrace,
    # we may need to return the Continuation and pass it upwards.
    continuation.execute()
