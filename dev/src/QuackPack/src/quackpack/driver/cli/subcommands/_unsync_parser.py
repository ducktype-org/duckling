from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.package import Package
from quackpack.driver.commands.unsync import UnsyncOptions, unsync
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.pkgid import Identifier

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``unsync`` command.

    :return: The parser configured for the ``unsync`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="unsync",
        description="Unsynchronize current or chosen venv by removing its state from the storage",
    ).add_str(long_name="--venv-id", help="Id of the venv to unsynchronize")


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``unsync`` subcommand to synchronize the current venv.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    """

    target: Package | Identifier
    if args.matched.venv_id is not None:
        target = Identifier(args.matched.venv_id)
    else:
        target = PackageLoader.find_from_cwd(ctx)
    opts = UnsyncOptions(ctx=ctx, target=target)
    unsync(opts)
