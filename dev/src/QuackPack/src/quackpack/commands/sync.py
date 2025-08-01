from dataclasses import dataclass

from quackpack.global_context import GlobalContext
from quackpack.package_loader import PackageLoader
from quackpack.storage.storage import venv_sync


@dataclass(frozen=True, kw_only=True)
class SyncOptions:
    ctx: GlobalContext

    overwrite: bool

    frozen: bool

    offline: bool

    sync_global: bool


def sync(opts: SyncOptions):
    overwrite = opts.overwrite
    if opts.sync_global:
        package = PackageLoader.global_package(opts.ctx)
        overwrite = True
    else:
        package = PackageLoader.find_from_cwd(opts.ctx)
    venv_sync(opts.ctx, package, overwrite=overwrite, frozen=opts.frozen, offline=opts.offline)
