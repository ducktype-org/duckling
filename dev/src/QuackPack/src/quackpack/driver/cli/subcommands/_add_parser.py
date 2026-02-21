from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.manifest.editable import Section
from quackpack.driver.commands.add import (
    AddOptions,
    NewDependencyType,
    add,
    parse_feature_flags,
)
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
        .add_flag(
            long_name="--global",
            short_name="-g",
            help="Add packages to the global venv instead",
        )
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(
                long_name="--local", help="Add dependencies as local dependencies"
            )
            .add_flag(long_name="--git", help="Add dependencies as git dependencies")
        )
        .add_str(
            long_name="--features",
            short_name="-F",
            help="Enable features for new packages",
            argument_count=ArgumentCount.ZeroOrMore,
        )
        .add_str(
            long_name="packages",
            argument_count=ArgumentCount.OneOrMore,
            help="Packages to add",
        )
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``add`` subcommand to add packages to the current package environment.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments args: Parsed command-line arguments for this subcommand.
    """

    package = (
        PackageLoader.global_package(ctx)
        if getattr(args.matched, "global")
        else PackageLoader.find_from_cwd(ctx)
    )
    packages: list[str] = args.matched.packages
    flags_: list[str] = args.matched.features or []
    flags = parse_feature_flags(flags_, ctx)
    section = Section.DEV_DEPS if args.matched.dev else Section.DEPS
    if args.matched.git:
        type_ = NewDependencyType.Git
    elif args.matched.local:
        type_ = NewDependencyType.Local
    else:
        type_ = NewDependencyType.Registry
    opts = AddOptions(
        packages=packages,
        type=type_,
        flags=flags,
        ctx=ctx,
        section=section,
        source=package,
    )
    add(opts)
