from contextlib import chdir

from quackpack.commands.init.file_creators import AdvancedProject, BasicProject
from quackpack.commands.init.types import InitOptions, NewVenvType
from quackpack.config.project import Manifest
from quackpack.project_loader import ProjectLoader
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger
from quackpack.util.pkgid import Identifier

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
            f"'{options.name}' is not valid project name, use '--name' to override"
        ) from None
    if options.ctx.console.quiet and options.type is NewVenvType.Full:
        # FIXME: If console is --quiet, then user won't see this message xD.
        raise QuackPackError("don't pass '--quiet' with '--full'")
    logger.debug(f"Creating new project '{options.name}' at '{options.destination}' of type {options.type!s}")
    try:
        options.destination.mkdir(parents=True, exist_ok=True)
    # https://docs.python.org/3/library/pathlib.html#pathlib.Path.mkdir
    except FileExistsError:
        raise QuackPackError(
            f"Destination '{options.destination}' already exists, but it's not a directory"
        ) from None
    manifest_destination = options.destination / ProjectLoader.MANIFEST_NAME
    # FIXME: Do we change configuration for libraries?
    basic_manifest = Manifest.create_basic_with_name(venv_id)
    try:
        basic_manifest.save_to(manifest_destination, mode="x")
    except FileExistsError:
        raise QuackPackError(f"Cannot reinitialize Venv at '{options.destination}'") from None
    if options.type is NewVenvType.PlainVenv:
        options.ctx.console.info(f"Successfully created new venv '{options.name}' at '{options.destination}'")
        return
    # FIXME: Should we rollback from errors after this points? Meaning we don't leave partially initialised venv...
    with chdir(options.destination):
        _populate_project_files(options, basic_manifest)


def _populate_project_files(options: InitOptions, manifest: Manifest) -> None:
    """
    Populate basic project files at CWD.
    ----
    Args:
    - `options`: init options.
    """
    assert options.type is not NewVenvType.PlainVenv, "PlainVenvs have no files to populate"
    match options.type:
        case NewVenvType.Binary:
            creator = BasicProject(options=options)
        case NewVenvType.Full:
            creator = AdvancedProject(options=options, manifest=manifest)
        case _:
            raise QuackPackError(f"Unknown Venv type '{options.type}'")
    creator.execute()
