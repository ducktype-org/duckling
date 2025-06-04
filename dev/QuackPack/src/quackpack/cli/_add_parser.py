from quackpack.commands.add import AddOptions, NewDependencyTable, NewDependencyType, add, parse_feature_flags
from quackpack.project_loader import ProjectLoader
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``add`` subcommand.

    :return: The parser configured for the ``add`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="add", description="Add packages to the current venv")
        .add_flag(long_name="--dev", help="Add packages as dev dependencies")
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--local", help="Add dependencies as local dependencies")
            .add_flag(long_name="--git", help="Add dependencies as git dependencies")
        )
        .add_str(
            long_name="--features",
            short_name="-F",
            help="Enable features for new packages",
            argument_count=ArgumentCount.ZeroOrMore,
        )
        .add_str(long_name="packages", argument_count=ArgumentCount.OneOrMore, help="Packages to add")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``add`` subcommand to add packages to the current project environment.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments args: Parsed command-line arguments for this subcommand.
    """

    project = ProjectLoader.find_from_cwd()
    packages: list[str] = args.matched.packages
    flags_: list[str] = args.matched.features or []
    flags = parse_feature_flags(flags_, ctx)
    table = NewDependencyTable.Dev if args.matched.dev else NewDependencyTable.Deps
    if args.matched.git:
        type = NewDependencyType.Git
    elif args.matched.local:
        type = NewDependencyType.Local
    else:
        type = NewDependencyType.Registry
    opts = AddOptions(packages=packages, type=type, flags=flags, ctx=ctx, table=table, source=project)
    add(opts)
