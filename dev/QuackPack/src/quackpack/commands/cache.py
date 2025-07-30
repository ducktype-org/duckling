import shutil
from dataclasses import dataclass

from quackpack.global_context import GlobalContext
from quackpack.signals import RobustSignalHandler
from quackpack.util.lock import FileLock
from quackpack.util.types.errors import QuackPackError


@dataclass(frozen=True, kw_only=True)
class CacheOptions:
    ctx: GlobalContext
    clean: bool


def cache(opts: CacheOptions):
    ctx = opts.ctx
    if opts.clean:
        with RobustSignalHandler(), FileLock(ctx.ensure_fetcher_lockfile()):
            shutil.rmtree(ctx.download_dir(), ignore_errors=True)
            shutil.rmtree(ctx.artifacts_dir(), ignore_errors=True)
            ctx.metadata_db().unlink(missing_ok=True)
    else:
        raise QuackPackError("No cache action specified")
