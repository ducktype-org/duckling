from quackpack.compile.compiler import CompilerArgs
from quackpack.core.package_loader import PackageLoader
from quackpack.driver.commands.build import BuildOptions, build
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
        CliParser.subcommand(name="build", description="Build a current package")
        .add_jobs()
        .add_exclusive_group(group=ExclusiveGroup().add_profile().add_release())
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
        .add_path(long_name="--entry", help="Entry path of the runned program")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``build`` subcommand to build the current package.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    """

    package = PackageLoader.find_from_cwd(ctx, allow_global=True)
    compiler_args = CompilerArgs()
    opts = BuildOptions(
        ctx=ctx,
        package=package,
        compiler_args=compiler_args,
        entry_path=args.matched.entry_path,
        duckc_binary=args.matched.duckc,
    )
    build(opts)
