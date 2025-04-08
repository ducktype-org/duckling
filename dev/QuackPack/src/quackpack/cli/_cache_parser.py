from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `cache` command.
    ----
    Returns:
    - `CliParser`: Parser for `cache` command.
    """
    return CliParser.subcommand(name="cache", description="Manage the cache").add_flag(
        long_name="--clean", short_name="-c", help="Clean the cache"
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'cache'")
    raise NotImplementedError
