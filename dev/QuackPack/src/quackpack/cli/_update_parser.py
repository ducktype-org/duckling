from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``update`` command.

    :return: The parser configured for the ``update`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="update", description="Update packages in the current venv").add_str(
        long_name="packages", help="Packages to update", argument_count=ArgumentCount.OneOrMore
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``update`` subcommand to update packages in the current venv.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    :raises NotImplementedError: Function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'update'")
    raise NotImplementedError
