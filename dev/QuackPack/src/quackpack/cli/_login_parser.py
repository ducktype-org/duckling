from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `login` command.
    ----
    Returns:
    - `CliParser`: Parser for `login` command.
    """
    return CliParser.subcommand(name="login", description="Login to the registry")


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'login'")
    raise NotImplementedError
