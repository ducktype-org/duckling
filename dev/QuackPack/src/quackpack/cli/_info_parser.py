from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``info`` subcommand.

    :return: The parser configured for the ``info`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="info", description="Get a package information").add_str(
        long_name="package", help="Package name"
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``info`` subcommand to retrieve information about a package.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'info'")
    raise NotImplementedError
