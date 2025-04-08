from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `update` command.
    ----
    Returns:
    - `CliParser`: Parser for `update` command.
    """
    return CliParser.subcommand(name="update", description="Update packages in the current venv").add_str(
        long_name="packages", help="Packages to update", multiple=True
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'update'")
    raise NotImplementedError
