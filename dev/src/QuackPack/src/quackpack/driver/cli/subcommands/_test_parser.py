from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``test`` command.

    :return: The parser configured for the ``test`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(
            name="test", description="Test a current package [NOT IMPLEMENTED]"
        )
        .add_jobs()
        .add_exclusive_group(group=ExclusiveGroup().add_profile().add_release())
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--locked", help="Assume freezefile doesn't change")
            .add_flag(long_name="--frozen", help="Don't update freezefile")
            .add_flag(long_name="--offline", help="Don't perform network requests")
        )
        .add_duckc()
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``test`` subcommand to test the current package.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    :raises NotImplementedError: Function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'test'")
    raise NotImplementedError
