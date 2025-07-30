from quackpack.commands.clean import CleanOptions, clean
from quackpack.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``clean`` subcommand.

    :return: The parser configured for the ``clean`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="clean",
        description="Clean all of the packages from the shared storage not pointed by any virtual environment",
    ).add_flag(long_name="--verbose", short_name="-v", help="Show all of the removed venvs and packages")


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``clean`` to clean all of the unused packages from the appropriate shared storage.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments args: Parsed command-line arguments for this subcommand.
    """

    opts = CleanOptions(ctx=ctx, verbose=args.matched.verbose)
    clean(opts)
