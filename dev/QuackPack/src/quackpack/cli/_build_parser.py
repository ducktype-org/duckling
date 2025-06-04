from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``build`` subcommand.

    :return: The parser configured for the ``build`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="build", description="Build a current project")
        .add_jobs()
        .add_exclusive_group(group=ExclusiveGroup().add_profile().add_release())
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--locked", help="Assume freezefile doesn't change")
            .add_flag(long_name="--frozen", help="Don't update freezefile")
            .add_flag(long_name="--offline", help="Don't perform network requests")
        )
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--all-features", help="Use all possible features")
            .add_str(
                long_name="--features",
                short_name="-F",
                argument_count=ArgumentCount.OneOrMore,
                help="Enable features of target package to build",
            )
        )
        .add_duckc()
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``build`` subcommand to build the current project.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    :raises NotImplementedError: This function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'build'")
    raise NotImplementedError
