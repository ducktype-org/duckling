from quackpack.util.global_context import GlobalContext

from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the argument parser for `sync` command.
    ----
    Returns:
    - `CliParser`: Parser for `sync` command.
    """
    return CliParser.subcommand(name="sync", description="Synchronize current venv").add_exclusive_group(
        group=ExclusiveGroup()
        .add_flag(long_name="--locked", help="Assume freezefile doesn't change")
        .add_flag(long_name="--frozen", help="Don't update freezefile")
        .add_flag(long_name="--offline", help="Don't perform network requests")
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'sync'")
    raise NotImplementedError
