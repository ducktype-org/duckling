from typing import Final

from ._parser import CliParser

EXTERNAL_CMD_PREFIX: Final[str] = "qp-"


def get_parser(name: str) -> CliParser:
    """
    Get the argument parser for one external subcommand.
    ----
    Args:
    - `name`: Name of external executable. Note that it must be in format `qp-{name}`, and argument should be stripped from `qp-` prefix.
    ----
    Returns:
    - `CliParser`: Parser for specified external subcommand.
    """
    return CliParser.subcommand(
        name=name, description=f"Run external cmd '{EXTERNAL_CMD_PREFIX}{name}'", with_help=False
    )
