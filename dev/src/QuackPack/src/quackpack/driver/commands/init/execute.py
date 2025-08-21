from contextlib import chdir

from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.manifest.editable import EditableManifest
from quackpack.core.types.manifest.schemas.manifest import ManifestSchema, MetadataSchema, SemverSchema
from quackpack.driver.commands.init.file_creators import AdvancedPackage, BasicPackage
from quackpack.driver.commands.init.types import InitOptions, NewVenvType
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

logger = get_logger(__name__)


def create_at(options: InitOptions) -> None:
    """
    Create new Venv at `destination`, with name `venv_name` of type `type`.
    ----
    Args:
    - `options`: init options.
    ----
    Raises:
    - `QuackPackError`: if `destination` points to file != directory, or there already exists Venv at `directory`.
    """
    try:
        venv_id = Identifier(options.name)
    except ValueError:
        raise QuackPackError(
            f"`{options.name}` is not a valid package name\nhint: use `--name` to override"
        ) from None
    if options.ctx.console.quiet and options.type is NewVenvType.Full:
        raise QuackPackError("don't pass '--quiet' with '--full'")
    logger.debug(f"Creating new package '{options.name}' at '{options.destination}' of type {options.type!s}")
    try:
        options.destination.mkdir(parents=True, exist_ok=True)
    # https://docs.python.org/3/library/pathlib.html#pathlib.Path.mkdir
    except FileExistsError:
        raise QuackPackError(
            f"package's root directory `{options.destination}` already exists, but it's not a directory"
        ) from None
    manifest_destination = options.destination / PackageLoader.MANIFEST_NAME
    metadata = MetadataSchema(version=SemverSchema(str(Version.default())), name=str(venv_id))
    manifest = ManifestSchema(metadata=metadata)
    saveable_manifest = EditableManifest.create_with_data(
        manifest.model_dump(exclude_none=True, exclude_unset=True)
    )
    try:
        with open(manifest_destination, mode="x") as f:
            f.write(saveable_manifest.as_str())
    except FileExistsError:
        raise QuackPackError(f"cannot reinitialize package at `{options.destination}`") from None
    if options.type is NewVenvType.PlainVenv:
        options.ctx.console.info(
            f"Successfully created new package `{options.name}` at `{options.destination}`"
        )
        return
    with chdir(options.destination):
        _populate_package_files(options, manifest)


def _populate_package_files(options: InitOptions, manifest: ManifestSchema) -> None:
    """
    Populate basic package files at CWD.
    ----
    Args:
    - `options`: init options.
    """
    assert options.type is not NewVenvType.PlainVenv, "PlainVenvs have no files to populate"
    match options.type:
        case NewVenvType.Binary:
            creator = BasicPackage(options=options)
        case NewVenvType.Full:
            creator = AdvancedPackage(options=options, manifest=manifest)
        case _:
            raise QuackPackError(f"unknown Venv type `{options.type}`")
    creator.execute()
