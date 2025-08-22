from quackpack.driver.commands.sync import SyncOptions, sync
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``sync`` command.

    :return: The parser configured for the ``sync`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="sync", description="Synchronize current venv")
        .add_flag(long_name="--frozen", help="Don't update freezefile")
        .add_flag(long_name="--offline", help="Don't perform network requests")
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(
                long_name="--overwrite",
                help="If a virtual environment of the same name exists in storage, forcefuly replace it",
            )
            .add_flag(
                long_name="--global", short_name="-g", help="Synchronize the global virtual environment"
            )
        )
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``sync`` subcommand to synchronize the current venv.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    """

    opts = SyncOptions(
        ctx=ctx,
        overwrite=args.matched.overwrite,
        frozen=args.matched.frozen,
        offline=args.matched.offline,
        sync_global=getattr(args.matched, "global"),
    )
    sync(opts)
