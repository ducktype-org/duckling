# NOTE: It turns out, that subparser type is private, so pyright goes crazy,
#       and that's why we force standard type checking.
# pyright: standard
import argparse
from pathlib import Path

from quackpack.global_info import GlobalInfo


def get_parser(formatter_class: type, prog: str) -> argparse.ArgumentParser:
    """
    Get the argument parser for `venv` command.
    ----
    Args:
    - `formatter_class`: Formatter class for this parser.
    - `prog`: Program name from main parser.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `venv` command.
    """
    parser = argparse.ArgumentParser(prog="venv", description="Manage venvs", formatter_class=formatter_class)
    parser.add_argument(
        "-p",
        "--path",
        nargs="?",
        default=Path.cwd(),
        type=Path,
        help="Path to the venv. Searches parent directories for a first venv",
    )
    # FIXME: Find better way to do this.
    # We need `prog`, so that all of the subparsers have `quackpack venv *CMD*` help messages, instead of `venv *CMD*`.
    subparser = parser.add_subparsers(
        title="Venv Commands", dest="venvaction", required=True, metavar="", prog=prog + " " + parser.prog
    )

    subparser.add_parser(
        "init", help="Initialize a new venv", formatter_class=parser.formatter_class, exit_on_error=False
    )

    add_parser = subparser.add_parser(
        "add",
        prog=None,
        help="Add packages to the venv",
        formatter_class=parser.formatter_class,
        exit_on_error=False,
    )
    add_parser.add_argument("packages", nargs="+", type=str, help="Packages to add")
    add_parser.add_argument("--dev", action="store_true", help="Add packages as dev dependencies")

    subparser.add_parser(
        "update",
        help="Update packages in the venv",
        formatter_class=parser.formatter_class,
        exit_on_error=False,
    ).add_argument("packages", nargs="*", type=str, help="Packages to update")

    subparser.add_parser(
        "remove",
        help="Remove packages from the venv",
        formatter_class=parser.formatter_class,
        exit_on_error=False,
    ).add_argument("packages", nargs="+", type=str, help="Packages to remove")

    subparser.add_parser(
        "sync", help="Synchronize the venv state", formatter_class=parser.formatter_class, exit_on_error=False
    )
    # We know its type, but the fact, that it's private somehow irritates pyright? So let's just silence it.
    _build_features(subparser, parser.formatter_class)  # pyright: ignore[reportArgumentType]
    _build_aliases(subparser, parser.formatter_class)  # pyright: ignore[reportArgumentType]
    return parser


def _build_features(subparser: argparse._SubParsersAction, formatter_class: type) -> None:
    """
    Build subparser for managing venv features.
    ----
    Args:
    - `subparser`: The subparser to which the venv features parser should be added.
    - `formatter_class`: Formatter class for subparsers.
    """
    features = subparser.add_parser(
        "feature", help="Manage venv features", formatter_class=formatter_class, exit_on_error=False
    ).add_subparsers(title="Feature Commands", dest="featureaction", required=True, metavar="")

    # You can't chain add_argument(), since they return some garbage.
    features_add = features.add_parser(
        "add", help="Add features from a package", formatter_class=formatter_class, exit_on_error=False
    )
    features_add.add_argument("package", type=str, help="Package to change")
    features_add.add_argument("features", nargs="+", type=str, help="Features to add")

    # You can't chain add_argument(), since they return some garbage.
    features_remove = features.add_parser(
        "remove", help="Remove features from a package", formatter_class=formatter_class, exit_on_error=False
    )
    features_remove.add_argument("package", type=str, help="Package to change")
    features_remove.add_argument("features", nargs="+", type=str, help="Features to remove")

    features.add_parser(
        "list",
        help="List features of specified packages",
        formatter_class=formatter_class,
        exit_on_error=False,
    ).add_argument("packages", nargs="+", type=str, help="Packages to list")


def _build_aliases(subparser: argparse._SubParsersAction, formatter_class: type) -> None:
    """
    Build subparser for managing venv aliases.
    ----
    Args:
    - `subparser`: The subparser to which the venv aliases parser should be added.
    - `formatter_class`: Formatter class for subparsers.
    """
    aliases = subparser.add_parser(
        "alias", help="Manage package aliases", formatter_class=formatter_class, exit_on_error=False
    ).add_subparsers(title="Alias Commands", dest="aliasaction", required=True, metavar="")

    # You can't chain add_argument(), since they return some garbage.
    add_alias = aliases.add_parser(
        "add", help="Add alias for the package", formatter_class=formatter_class, exit_on_error=False
    )
    add_alias.add_argument("package", type=str, help="Package to change")
    add_alias.add_argument("alias", type=str, help="Alias to add")

    aliases.add_parser(
        "remove", help="Remove package aliases", formatter_class=formatter_class, exit_on_error=False
    ).add_argument("aliases", nargs="+", type=str, help="Alias to remove")

    aliases.add_parser(
        "list", help="List all package aliases", formatter_class=formatter_class, exit_on_error=False
    )


def execute(info: GlobalInfo) -> int:
    """
    Execute this subcommand.
    ----
    Args:
    - `info`: all possibly needed information for this function.
    ----
    Returns:
    - `int`: return code for main.
    """
    info.console.debug("Implement 'execute()' for 'venv'")
    raise NotImplementedError
