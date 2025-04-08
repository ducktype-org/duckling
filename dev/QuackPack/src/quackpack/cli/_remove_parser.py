from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `remove` command.
    ----
    Returns:
    - `CliParser`: Parser for `remove` command.
    """
    return CliParser.subcommand(name="remove", description="Remove packages from the current venv").add_str(
        long_name="packages", multiple=True, help="Packages to remove"
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'remove'")
    raise NotImplementedError
