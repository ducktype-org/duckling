import os
from typing import Final, Never

from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

from ._parser import CliParser

logger = get_logger(__name__)

EXTERNAL_CMD_PREFIX: Final[str] = "qp-"


def get_parser(name: str) -> CliParser:
    """
    Get the CLI argument parser for an external subcommand.

    :param str name: The name of the external executable (without the ``qp-`` prefix).
    :return: The parser configured for the specified external subcommand.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name=name, description=f"Run external cmd '{EXTERNAL_CMD_PREFIX}{name}'", with_help=False
    )


def handle_external_command(name: str, cmd_args: list[str]) -> Never:
    """
    Execute an external subcommand by invoking a ``qp-{name}`` executable.

    :param str name: The name of the external subcommand to execute.
    :param list[str] cmd_args: A list of arguments to pass to the external subcommand.
    :raises quackpack.util.errors.QuackPackError: If execution of the external command fails.
    :rtype: Never
    """

    full_name = EXTERNAL_CMD_PREFIX + name
    # argv[0] always should be executable name.
    cmd_args.insert(0, full_name)
    logger.debug(f"Executing external command '{name}' with arguments {cmd_args}")
    os.execvp(full_name, cmd_args)
    raise QuackPackError("Exec failed")
