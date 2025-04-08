from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `ctx` command.
    ----
    Returns:
    - `CliParser`: Parser for `info` command.
    """
    return CliParser.subcommand(name="info", description="Get a package information").add_str(
        long_name="package", help="Package name"
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'info'")
    raise NotImplementedError
