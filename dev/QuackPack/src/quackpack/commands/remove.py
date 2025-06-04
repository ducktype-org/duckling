from dataclasses import dataclass

from quackpack.project import Project
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.pkgid import Identifier

logger = get_logger(__name__)


@dataclass(frozen=True, kw_only=True)
class RemoveOptions:
    ctx: GlobalContext
    project: Project
    """
    Project, from which dependencies will be removed.
    """

    to_remove: list[str]
    """
    Dependencies to remove.
    """


def remove(options: RemoveOptions) -> None:
    with options.project.lock():
        _remove_with_lock_held(options)


# FIXME(yaml): Bump to edytowalny manifest.
# FIXME: Support dry run.
def _remove_with_lock_held(options: RemoveOptions) -> None:
    removed_count = 0
    ctx = options.ctx
    project = options.project
    logger.debug(f"Removing packages '{options.to_remove}' from project at '{project.manifest_path}'")
    manifest = project.manifest_without_acquiring_lock()
    for package in options.to_remove:
        try:
            package = Identifier(package)
        except Exception as e:
            logger.debug(f"{package} is not a valid identifier, skipping")
            ctx.console.warn(str(e))
            continue
        if package not in manifest.dependencies:
            logger.debug(f"{package} is not a dependency, skipping")
            ctx.console.warn(f"There is no such dependency as '{package}'")
            continue
        del manifest.dependencies[package]
        ctx.console.info(f"Removed dependency '{package}'", verbose=True)
        removed_count += 1
    if removed_count == 0:
        ctx.console.info("Didn't remove any dependencies")
    elif removed_count == 1:
        ctx.console.info("Removed 1 dependency")
    else:
        ctx.console.info(f"Removed {removed_count} dependencies")
    project.save_to_disk()
