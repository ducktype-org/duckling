from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``login`` subcommand.

    :return: The parser configured for the ``login`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="login", description="Login to the registry [NOT IMPLEMENTED]"
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``login`` subcommand to authenticate with the registry.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'login'")
    raise NotImplementedError
