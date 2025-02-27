import os
from argparse import ArgumentParser
from collections import defaultdict
from collections.abc import Callable
from pathlib import Path
from typing import Final

from quackpack.global_info import GlobalInfo

from . import (
    _build_parser,
    _cache_parser,
    _external_cmd_parser,
    _info_parser,
    _init_parser,
    _login_parser,
    _logout_parser,
    _publish_parser,
    _run_parser,
    _search_parser,
    _test_parser,
    _unpublish_parser,
    _venv_parser,
)
from ._main_parser import get_early_parser

__all__ = ["ExecFn", "action_for", "get_early_parser", "get_parser"]

type ExecFn = Callable[[GlobalInfo], int]


def get_parser() -> ArgumentParser:
    """
    Get the main argument parser for Quack Pack.
    ----
    Returns:
    - `argparse.ArgumentParser`: The fully combined argument parser.
    """
    parser = get_early_parser()
    # We need to pass formatter_class down, because of venv subparsers.
    # FIXME: This may be nicer, if deriving formatter_class's would work.
    subcommands = _cli(parser.formatter_class, parser.prog)  # pyright: ignore [reportArgumentType]
    subparser = parser.add_subparsers(title="Commands", dest="command", metavar="")
    for command in subcommands:
        subparser.add_parser(
            command.prog,
            help=command.description,
            aliases=_builtin_aliases()[command.prog],
            parents=[command],
            formatter_class=parser.formatter_class,
            exit_on_error=False,
            add_help=False,
        )
    return parser


def _cli(formatter_class: type, prog: str) -> list[ArgumentParser]:
    """
    Get list of all known subparsers for Quack Pack.
    ----
    Args:
    - `formatter_class`: Formatter class for subparsers of returned parsers.
    - `prog`: Program name from main parser.
    ----
    Returns:
    - `list[argparse.ArgumentParser]`: List of all known subparsers.
    """
    subcommands = [
        _build_parser.get_parser(),
        _run_parser.get_parser(),
        _test_parser.get_parser(),
        _cache_parser.get_parser(),
        _venv_parser.get_parser(formatter_class, prog),
        _init_parser.get_parser(),
        _info_parser.get_parser(),
        _search_parser.get_parser(),
        _login_parser.get_parser(),
        _logout_parser.get_parser(),
        _publish_parser.get_parser(),
        _unpublish_parser.get_parser(),
    ]
    _update_with_external_cmds(subcommands)
    return subcommands


def _update_with_external_cmds(buitlin_commands: list[ArgumentParser]) -> None:
    """
    Update Quack Pack subcommands with external executables.
    ----
    Args:
    - `buitlin_commands`: list of subparsers which will be populated with external executables.
    """
    paths = (path for x in os.environ["PATH"].split(":") if (path := Path(x)).is_dir())
    known_commands = {parser.prog for parser in buitlin_commands}
    PREFIX: Final[str] = "qp-"
    for dir in paths:
        for x in dir.iterdir():
            # Skip non executable files.
            if not (x.is_file() and os.access(x, os.X_OK)):
                continue
            basename = x.name
            if not basename.startswith(PREFIX):
                continue
            command_name = basename.removeprefix(PREFIX)
            if command_name in known_commands:
                continue
            buitlin_commands.append(_external_cmd_parser.get_parser(command_name))
            known_commands.add(command_name)


def _builtin_aliases() -> defaultdict[str, list[str]]:
    """
    Get default Quack Pack command aliases.
    ----
    Returns:
    - `defaultdict[str, list[str]]`: Dictionary with mapping `command` -> `its aliases`.
    """
    return defaultdict(list, {"build": ["b"], "run": ["r"], "test": ["t"]})


def action_for(command: str) -> ExecFn | None:
    """
    All possible actions for subcommands.
    ----
    Args:
    - `command`: name of the `command`.
    ----
    Returns:
    - `ExecFn | None`: possible function which executes provided subcommand.
    """
    match command:
        case "build":
            return _build_parser.execute
        case "cache":
            return _cache_parser.execute
        case "info":
            return _info_parser.execute
        case "init":
            return _init_parser.execute
        case "login":
            return _login_parser.execute
        case "logout":
            return _logout_parser.execute
        case "publish":
            return _publish_parser.execute
        case "run":
            return _run_parser.execute
        case "search":
            return _search_parser.execute
        case "test":
            return _test_parser.execute
        case "unpublish":
            return _unpublish_parser.execute
        case "venv":
            return _venv_parser.execute
        case _:
            return None
