from quackpack.core.package_loader import PackageLoader
from quackpack.driver.commands.publish import PublishOptions, publish
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``publish`` subcommand.

    :return: The parser configured for the ``publish`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="publish", description="Publish package to the registry"
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``publish`` subcommand to publish a package to the registry.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    """

    package = PackageLoader.find_from_cwd(ctx)
    opts = PublishOptions(ctx=ctx, source=package)
    publish(opts)
