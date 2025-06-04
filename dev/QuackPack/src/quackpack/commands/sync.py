from dataclasses import dataclass

from quackpack.project import Project
from quackpack.storage import venv_sync
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class SyncOptions:
    ctx: GlobalContext

    target: Project
    """
    Project to be synced.
    """


def sync(opts: SyncOptions):
    venv_sync(opts.ctx, opts.target)
