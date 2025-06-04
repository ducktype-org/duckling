from quackpack.commands.sync import SyncOptions, sync
from quackpack.project_loader import ProjectLoader
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``sync`` command.

    :return: The parser configured for the ``sync`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="sync", description="Synchronize current venv").add_exclusive_group(
        group=ExclusiveGroup()
        .add_flag(long_name="--locked", help="Assume freezefile doesn't change")
        .add_flag(long_name="--frozen", help="Don't update freezefile")
        .add_flag(long_name="--offline", help="Don't perform network requests")
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``sync`` subcommand to synchronize the current venv.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    """

    project = ProjectLoader.find_from_cwd()
    opts = SyncOptions(ctx=ctx, target=project)
    sync(opts)
