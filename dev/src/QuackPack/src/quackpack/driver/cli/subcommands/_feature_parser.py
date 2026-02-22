"""
TODO
Note that this subcommand is currently not included in argparser.

The subcommand is meant to modify the `features` part of the manifest from CLI,
but as it is not critical and it is not implemented, we do not include it in parser.
"""

from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser


def get_parser(prog: str) -> CliParser:
    """
    Get the CLI argument parser for the ``feature`` subcommand.

    :param str prog: The name of the program, used for displaying usage in help messages.
    :return: The parser configured for the ``feature`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="feature", description="Manage features of packages in the current venv"
    ).add_subcommands(
        CliParser.subcommand(name="add", description="Add features of the package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="feature", argument_count=ArgumentCount.OneOrMore, help="Features to add"),
        CliParser.subcommand(name="remove", description="Remove features from a package")
        .add_str(long_name="package", help="Package to change")
        .add_str(long_name="feature", argument_count=ArgumentCount.OneOrMore, help="Features to remove"),
        CliParser.subcommand(name="list", description="List features of specified packages").add_str(
            long_name="packages", argument_count=ArgumentCount.OneOrMore, help="Packages to list"
        ),
        title="Feature Commands",
        destination="action",
        prog=f"{prog} feature",
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``feature`` subcommand to manage package features.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'feature'")
    raise NotImplementedError
