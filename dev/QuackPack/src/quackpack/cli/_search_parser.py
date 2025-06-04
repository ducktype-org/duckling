from quackpack.commands.search import SearchOptions, search
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``search`` command.

    :return: The parser configured for the ``search`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(name="search", description="Search for a package in the registry").add_str(
        long_name="package", help="Package name"
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``search`` subcommand to search for a package in the registry.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments args: Parsed arguments from the CLI for this command.
    """

    query: str = args.matched.package
    opts = SearchOptions(ctx=ctx, query=query)

    result = search(opts)

    # TODO: FIXME: Add output formating - something like
    #
    # > Found {n} packages for query '{query}':
    # > pkg1
    # > pkg2
    # > ...
    #
    # where {n}, {query} are somehow colored

    ctx.console.info(result)
