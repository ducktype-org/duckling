from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser(prog: str) -> CliParser:
    """
    Get the argument parser for `alias` command.
    ----
    Args:
    - `prog`: Program name from main parser.
    ----
    Returns:
    - `CliParser`: Parser for `alias` command.
    """
    return CliParser.subcommand(
        name="alias", description="Manage aliases of packages in the currrent venv"
    ).add_subcommands(
        CliParser.subcommand(name="add", description="Add alias to the package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="alias", help="Alias to add"),
        CliParser.subcommand(name="remove", description="Remove packages aliases").add_str(
            long_name="alias", multiple=True, help="Aliases to remove"
        ),
        CliParser.subcommand(name="list", description="List aliases of all packages"),
        title="Alias Commands",
        destination="action",
        prog=f"{prog} alias",
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'alias'")
    raise NotImplementedError
