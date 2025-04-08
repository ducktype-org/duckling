from contextlib import chdir

from quackpack.commands.init.file_creators import AdvancedProject, BasicProject
from quackpack.commands.init.types import InitOptions, NewVenvType
from quackpack.config.project import Venv
from quackpack.config.project.models import Configuration, Metadata
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

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
    if not Metadata.is_valid_project_name(options.name):
        raise QuackPackError(f"'{options.name}' is not valid project name, use '--name' to override")
    if options.ctx.console.quiet and options.type is NewVenvType.Full:
        # FIXME: If console is --quiet, then user won't see this message xD.
        raise QuackPackError("don't pass '--quiet' with '--full'")
    logger.debug(f"Creating new Venv '{options.name}' at '{options.destination}' of type {options.type}")
    try:
        options.destination.mkdir(parents=True, exist_ok=True)
    # https://docs.python.org/3/library/pathlib.html#pathlib.Path.mkdir
    except FileExistsError:
        raise QuackPackError(
            f"Destination '{options.destination}' already exists, but it's not a directory"
        ) from None
    config_destination = options.destination / Venv.CONFIG_FILE_NAME
    # FIXME: Do we change configuration for libraries?
    basic_config = Configuration.create_basic_with_name(options.name)
    try:
        basic_config.save_to(config_destination, mode="x")
    except FileExistsError:
        raise QuackPackError(f"Cannot reinitialize Venv at '{options.destination}'") from None
    if options.type is NewVenvType.PlainVenv:
        options.ctx.console.info(f"Successfully created new venv '{options.name}' at '{options.destination}'")
        return
    with chdir(options.destination):
        _populate_project_files(options, basic_config)


def _populate_project_files(options: InitOptions, project_config: Configuration) -> None:
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
            creator = AdvancedProject(options=options, project_config=project_config)
        case _:
            raise QuackPackError(f"Unknown Venv type '{options.type}'")
    creator.execute()
