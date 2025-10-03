from quackpack.driver.commands.list import ListOptions, VenvListSortKey, list_
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.errors import QuackPackError

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``login`` subcommand.

    :return: The parser configured for the ``login`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="list", description="List all virtual environments")
        .add_str(
            long_name="--sort-by",
            default="name",
            help='Properties to sort the output by. Can be any of "name", "last_access", "last_modification".',
        )
        .add_flag(
            long_name="--sort-reverse",
            help="Controls if the sort should be done in reverse order",
        )
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``list`` to list all virtual environments.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments _args: Parsed command-line arguments for this subcommand.
    """

    sort_key = _str_to_sort_key(args.matched.sort_by)
    opts = ListOptions(ctx=ctx, key=sort_key, reverse=args.matched.sort_reverse)
    list_(opts)


def _str_to_sort_key(value: str) -> VenvListSortKey:
    match value.lower():
        case "name":
            return VenvListSortKey.NAME
        case "last_access":
            return VenvListSortKey.LAST_ACCESS
        case "last_modification":
            return VenvListSortKey.LAST_MODIFICATION
        case _:
            raise QuackPackError(f"Invalid sort-by key: {value}")
