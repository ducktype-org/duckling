from quackpack.util.global_context import GlobalContext

from ._parser import CliParser


def get_parser(prog: str) -> CliParser:
    """
    Get the argument parser for `feature` command.
    ----
    Args:
    - `prog`: Program name from main parser.
    ----
    Returns:
    - `CliParser`: Parser for `feature` command.
    """
    return CliParser.subcommand(
        name="feature", description="Manage features of packages in the current venv"
    ).add_subcommands(
        CliParser.subcommand(name="add", description="Add features of the package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="feature", multiple=True, help="Features to add"),
        CliParser.subcommand(name="remove", description="Remove features from a package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="feature", multiple=True, help="Feautres to remove"),
        CliParser.subcommand(name="list", description="List features of specified packages").add_str(
            long_name="packages", multiple=True, help="Packages to list"
        ),
        title="Feature Commands",
        destination="action",
        prog=f"{prog} feature",
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'feature'")
    raise NotImplementedError
