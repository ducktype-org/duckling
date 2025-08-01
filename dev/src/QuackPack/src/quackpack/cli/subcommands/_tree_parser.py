from quackpack.commands.tree import TreeOptions, tree
from quackpack.global_context import GlobalContext
from quackpack.package_loader import PackageLoader
from quackpack.util.types.errors import QuackPackError

from ._arguments import Arguments
from ._parser import CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``tree`` command.

    :return: The parser configured for the ``tree`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.subcommand(name="tree", description="Print the dependency tree of a package")
        .add_flag(long_name="--no-dedup", help="Show the subtree of a package everytime")
        .add_int(long_name="--max-depth", help="Set the maximal displayed depth of the tree")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    if args.matched.max_depth is not None and args.matched.max_depth < 1:
        raise QuackPackError("The maximal depth should be at least one")
    project = PackageLoader.find_from_cwd(ctx)
    tree(
        TreeOptions(
            ctx=ctx, source=project, max_depth=args.matched.max_depth, expand_visited=args.matched.no_dedup
        )
    )
