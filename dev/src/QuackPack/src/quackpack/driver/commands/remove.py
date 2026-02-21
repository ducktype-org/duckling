from dataclasses import dataclass

from quackpack.core.types.manifest.editable import EditableManifest, Section
from quackpack.core.types.package import Package
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


@dataclass(frozen=True, kw_only=True)
class RemoveOptions:
    ctx: GlobalContext
    package: Package
    """
    Package, from which dependencies will be removed.
    """

    to_remove: list[str]
    """
    Dependencies to remove.
    """

    section: Section
    """
    Which type of dependencies to remove.
    """


def remove(options: RemoveOptions) -> None:
    removed_count = 0
    ctx = options.ctx
    package = options.package
    logger.debug(
        f"Removing packages `{options.to_remove}` from package at `{package.manifest_path}`"
    )
    manifest = package.manifest
    editable = EditableManifest.load(manifest.original_content)
    table = editable.get_table(options.section)
    if table is None:
        ctx.error_console.warn(
            f"package with manifest at `{package.manifest_path!s}` has no `{options.section.as_str()}`"
        )
        return
    for new_dep in options.to_remove:
        if not is_valid_identifier(new_dep):
            ctx.error_console.warn(f"`{new_dep}` is not a valid identifier, skipping")
            continue
        if new_dep not in table:
            ctx.error_console.warn(f"There is no such dependency as `{new_dep}`")
            continue
        del table[new_dep]
        ctx.console.info(f"Removed dependency `{new_dep}`", verbose=True)
        removed_count += 1
    if removed_count == 0:
        ctx.console.info("Didn't remove any dependencies")
    elif removed_count == 1:
        ctx.console.info("Removed 1 dependency")
    else:
        ctx.console.info(f"Removed {removed_count} dependencies")
    with package.lock():
        package.manifest_path.write_text(editable.as_str())
