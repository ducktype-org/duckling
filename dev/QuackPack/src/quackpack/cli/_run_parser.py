from quackpack.commands.run import RunOptions, run
from quackpack.compile import CompilerArgs
from quackpack.project_loader import ProjectLoader
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``run`` command.

    :return: The parser configured for the ``run`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="run", description="Build a current project and run it")
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
        .add_str(
            long_name="args",
            argument_count=ArgumentCount.ZeroOrMore,
            help="Arguments passed to compiled binary",
        )
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``run`` subcommand to build the current project and run it.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments args: Parsed arguments from the CLI for this command.
    """

    project = ProjectLoader.find_from_cwd()
    compiler_args = CompilerArgs()
    opts = RunOptions(ctx=ctx, package=project, compiler_args=compiler_args, exec_args=args.matched.args)
    run(opts)
