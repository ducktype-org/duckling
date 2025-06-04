from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``cache`` subcommand.

    :return: The parser configured for the ``cache`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="cache", description="Manage the cache").add_flag(
        long_name="--clean", short_name="-c", help="Clean the cache"
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``cache`` subcommand to manage the local cache.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'cache'")
    raise NotImplementedError
