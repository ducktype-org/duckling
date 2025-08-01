from quackpack.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``logout`` subcommand.

    :return: The parser configured for the ``logout`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="logout", description="Logout from the registry [NOT IMPLEMENTED]")


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``logout`` subcommand to log out from the registry.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'logout'")
    raise NotImplementedError
