from dataclasses import dataclass

from quackpack.global_context import GlobalContext
from quackpack.package import Package
from quackpack.storage.storage import venv_delete
from quackpack.util.types.pkgid import Identifier


@dataclass(frozen=True, kw_only=True)
class UnsyncOptions:
    ctx: GlobalContext

    target: Package | Identifier
    """
    Package to be unsynced.
    """


def unsync(opts: UnsyncOptions):
    venv_delete(opts.ctx, opts.target)
