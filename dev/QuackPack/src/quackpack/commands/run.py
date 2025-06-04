from dataclasses import dataclass
from pathlib import Path

from quackpack.compile import CompileAndRun, CompilerArgs
from quackpack.project import Project
from quackpack.storage import run as run_impl
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class RunOptions:
    ctx: GlobalContext

    package: Project
    """
    Project to be compiled and run.
    """

    compiler_args: CompilerArgs

    exec_args: list[str]


def run(opts: RunOptions):
    runner = CompileAndRun(opts.compiler_args, opts.exec_args)
    continuation = run_impl(opts.ctx, opts.package, runner, Path())
    # TODO (Artur) na moją intuicję Continuation powinno być
    # przekazywane w górę, bo natychmiast kończy program, a może
    # być coś do wyczyszczenia na górze (ale być może u nas nie
    # ma nic takiego i możemy tutaj po prostu zrobić `execute`)
    continuation.execute()
