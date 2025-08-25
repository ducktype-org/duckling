from dataclasses import dataclass

from quackpack.core.storage.storage import clean_storage
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class CleanOptions:
    ctx: GlobalContext
    verbose: bool


def clean(opts: CleanOptions):
    clean_output = clean_storage(opts.ctx)
    if opts.verbose:
        display_removed_venvs = sorted(str(id) for id in clean_output.removed_venvs)
        opts.ctx.console.info(
            f"States of the following ephemeral virtual environments have been removed from the storage: {display_removed_venvs}"
        )
        display_removed_packages = sorted(path.name for path in clean_output.removed_packages)
        opts.ctx.console.info(
            f"Packages from the following localizations have been removed from the storage: {display_removed_packages}"
        )
