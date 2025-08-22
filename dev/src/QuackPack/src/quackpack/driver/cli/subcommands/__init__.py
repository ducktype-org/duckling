import os
from collections import defaultdict
from collections.abc import Callable
from pathlib import Path

from quackpack.util.global_context import GlobalContext

from . import (
    _add_parser,
    _build_parser,
    _cache_parser,
    _clean_parser,
    _external_cmd_parser,
    _info_parser,
    _init_parser,
    _list_parser,
    _login_parser,
    _logout_parser,
    _publish_parser,
    _remove_parser,
    _run_parser,
    _search_parser,
    _sync_parser,
    _test_parser,
    _tree_parser,
    _unsync_parser,
)
from ._arguments import Arguments
from ._external_cmd_parser import EXTERNAL_CMD_PREFIX, handle_external_command
from ._main_parser import get_early_parser
from ._parser import CliParser

__all__ = [
    "EXTERNAL_CMD_PREFIX",
    "Arguments",
    "ExecFn",
    "action_for",
    "builtin_aliases",
    "get_parser",
    "handle_external_command",
    "setup_parser",
    "subcommands",
]


type ExecFn = Callable[[GlobalContext, Arguments], None]


def setup_parser() -> None:
    """
    Setup any parser related utilities.
    """
    CliParser.setup_formatter()


def get_parser() -> CliParser:
    """
    Get the main argument parser for Quack Pack.
    ----
    Returns:
    - `CliParser`: The fully combined argument parser.
    """
    parser = get_early_parser()
    return parser.add_subcommands(
        *subcommands(parser.prog),
        title="Commands",
        destination="command",
        required=False,
        alias_set_default_workaround=True,
        aliases=builtin_aliases(),
    )


def subcommands(_prog: str) -> list[CliParser]:
    """
    Get list of all known subparsers for Quack Pack.
    ----
    Args:
    - `prog`: Program name from main parser.
    ----
    Returns:
    - `list[CliParser]`: List of all known subparsers.
    """
    subcommands = [
        _build_parser.get_parser(),
        _run_parser.get_parser(),
        _test_parser.get_parser(),
        _add_parser.get_parser(),
        _remove_parser.get_parser(),
        _sync_parser.get_parser(),
        _unsync_parser.get_parser(),
        _list_parser.get_parser(),
        _cache_parser.get_parser(),
        _init_parser.get_parser(),
        _info_parser.get_parser(),
        _search_parser.get_parser(),
        _login_parser.get_parser(),
        _logout_parser.get_parser(),
        _publish_parser.get_parser(),
        _clean_parser.get_parser(),
        _tree_parser.get_parser(),
    ]
    _update_external_cmds(subcommands)
    return subcommands


def _update_external_cmds(builtin_commands: list[CliParser]) -> None:
    """
    Update Quack Pack subcommands with external executables.
    ----
    Args:
    - `builtin_commands`: list of subparsers which will be populated with external executables.
    """
    paths = (path for x in os.environ["PATH"].split(":") if (path := Path(x)).is_dir())
    known_commands = {parser.prog for parser in builtin_commands}
    for directory in paths:
        for x in directory.iterdir():
            # Skip non executable files.
            if not (os.access(x, os.X_OK) and x.is_file()):
                continue
            basename = x.name
            if not basename.startswith(EXTERNAL_CMD_PREFIX):
                continue
            command_name = basename.removeprefix(EXTERNAL_CMD_PREFIX)
            if command_name in known_commands:
                continue
            builtin_commands.append(_external_cmd_parser.get_parser(command_name))
            known_commands.add(command_name)


def builtin_aliases() -> defaultdict[str, list[str]]:
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
        case "add":
            return _add_parser.execute
        case "build":
            return _build_parser.execute
        case "cache":
            return _cache_parser.execute
        case "clean":
            return _clean_parser.execute
        case "info":
            return _info_parser.execute
        case "init":
            return _init_parser.execute
        case "list":
            return _list_parser.execute
        case "login":
            return _login_parser.execute
        case "logout":
            return _logout_parser.execute
        case "publish":
            return _publish_parser.execute
        case "remove":
            return _remove_parser.execute
        case "run":
            return _run_parser.execute
        case "search":
            return _search_parser.execute
        case "sync":
            return _sync_parser.execute
        case "test":
            return _test_parser.execute
        case "tree":
            return _tree_parser.execute
        case "unsync":
            return _unsync_parser.execute
        case _:
            return None
