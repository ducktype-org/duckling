from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the argument parser for `add` command.
    ----
    Returns:
    - `CliParser`: Parser for `add` command.
    """
    return (
        CliParser.subcommand(name="add", description="Add packages to the current venv")
        .add_flag(long_name="--dev", help="Add packages as dev dependencies")
        .add_str(long_name="packages", multiple=True, help="Packages to add")
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'add'")
    raise NotImplementedError
