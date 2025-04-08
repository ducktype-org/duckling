from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `logout` command.
    ----
    Returns:
    - `CliParser`: Parser for `logout` command.
    """
    return CliParser.subcommand(name="logout", description="Logout from the registry")


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'logout'")
    raise NotImplementedError
