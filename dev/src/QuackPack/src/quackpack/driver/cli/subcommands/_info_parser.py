from quackpack.driver.commands.info import InfoOptions, info
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.version import Version

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``info`` subcommand.

    :return: The parser configured for the ``info`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="info", description="Get a package information")
        .add_str(long_name="package", help="Package name")
        .add_str(long_name="version", help="Package version")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``info`` subcommand to retrieve information about a package.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    """

    package_name: str = args.matched.package
    package_version: str = args.matched.version

    opts = InfoOptions(
        ctx=ctx,
        package_name=package_name,
        package_version=Version.create_from_string(package_version),
    )

    info(opts)
