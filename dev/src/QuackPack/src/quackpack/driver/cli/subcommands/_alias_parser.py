"""
TODO
Note that this subcommand is currently not included in argparser.

The subcommand has originated during the time, when aliases were meant to work somewhat differently.
Some analouge of it will still be useful for full manifest manipulation capabilities from cli,
but it needs conceptual rework (and then implementation).
"""

from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser


def get_parser(prog: str) -> CliParser:
    """
    Get the CLI argument parser for the ``alias`` subcommand.

    :param str prog: The name of the program, used for displaying usage in help messages.
    :return: The parser configured for the ``alias`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="alias", description="Manage aliases of packages in the current venv"
    ).add_subcommands(
        CliParser.subcommand(name="add", description="Add alias to the package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="alias", help="Alias to add"),
        CliParser.subcommand(
            name="remove", description="Remove packages aliases"
        ).add_str(
            long_name="alias",
            argument_count=ArgumentCount.OneOrMore,
            help="Aliases to remove",
        ),
        CliParser.subcommand(name="list", description="List aliases of all packages"),
        title="Alias Commands",
        destination="action",
        prog=f"{prog} alias",
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``alias`` subcommand.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'alias'")
    raise NotImplementedError
