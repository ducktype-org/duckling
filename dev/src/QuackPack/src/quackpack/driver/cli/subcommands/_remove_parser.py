from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.manifest.editable import Section
from quackpack.driver.commands.remove import RemoveOptions, remove
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``remove`` command.

    :return: The parser configured for the ``remove`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="remove", description="Remove packages from the current venv")
        .add_str(long_name="packages", argument_count=ArgumentCount.OneOrMore, help="Packages to remove")
        .add_flag(long_name="--global", short_name="-g", help="Add packages to the global venv instead")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``remove`` subcommand to remove packages from the current package environment.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments args: Parsed arguments from the CLI for this command.
    """

    package = (
        PackageLoader.global_package(ctx)
        if getattr(args.matched, "global")
        else PackageLoader.find_from_cwd(ctx)
    )
    # FIXME: Support dev-deps.
    options = RemoveOptions(ctx=ctx, to_remove=args.matched.packages, package=package, section=Section.DEPS)
    remove(options)
