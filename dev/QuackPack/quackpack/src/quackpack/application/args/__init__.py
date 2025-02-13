import os
from argparse import ArgumentParser
from collections import defaultdict
from pathlib import Path
from typing import Final

from . import (
    _build_parser,
    _cache_parser,
    _external_cmd_parser,
    _info_parser,
    _init_parser,
    _login_parser,
    _logout_parser,
    _main_parser,
    _publish_parser,
    _run_parser,
    _search_parser,
    _test_parser,
    _unpublish_parser,
    _venv_parser,
)

__all__ = ["builtin_aliases", "cli", "get_parser"]


def get_parser() -> ArgumentParser:
    """
    Get the main argument parser for Quack Pack.
    ----
    Returns:
    - `argparse.ArgumentParser`: The fully combined argument parser.
    """
    subcommands = cli()
    parser = _main_parser.get_early_parser()
    subparser = parser.add_subparsers(dest="command")
    for command in subcommands:
        subparser.add_parser(
            command.prog,
            help=command.description,
            aliases=builtin_aliases()[command.prog],
            parents=[command],
            formatter_class=parser.formatter_class,
            add_help=False,
        )
    # FIXME: Allow somehow `--` as a positional argument for executing scripts.
    return parser


# TODO: Maybe each `_*name*_parser.py` should also provide `exec(...) -> ...` function,
#       which would tell us, what action we should take.
#       (just like Cargo: https://github.com/rust-lang/cargo/blob/master/src/bin/cargo/commands/mod.rs)


def cli() -> list[ArgumentParser]:
    """
    Get list of all known subparsers for Quack Pack.
    ----
    Returns:
    - `list[argparse.ArgumentParser]`: List of all known subparsers.
    """
    subcommands = [
        _build_parser.get_parser(),
        _run_parser.get_parser(),
        _test_parser.get_parser(),
        _cache_parser.get_parser(),
        _venv_parser.get_parser(),
        _init_parser.get_parser(),
        _info_parser.get_parser(),
        _search_parser.get_parser(),
        _login_parser.get_parser(),
        _logout_parser.get_parser(),
        _publish_parser.get_parser(),
        _unpublish_parser.get_parser(),
    ]
    _update_with_external_cmds(subcommands)
    # TODO: We might want not to do this.
    subcommands.sort(key=lambda cmd: cmd.prog)
    return subcommands


def _update_with_external_cmds(buitlin_commands: list[ArgumentParser]) -> None:
    """
    Update Quack Pack subcommands with external executables.
    ----
    Args:
    - `buitlin_commands`: list of subparsers which will be populated with external executables.
    """
    paths = (Path(x) for x in os.environ["PATH"].split(":"))
    known_commands = {parser.prog for parser in buitlin_commands}
    PREFIX: Final[str] = "qp-"
    for dir in paths:
        assert dir.is_dir()
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


def builtin_aliases() -> defaultdict[str, list[str]]:
    """
    Get default Quack Pack command aliases.
    ----
    Returns:
    - `defaultdict[str, list[str]]`: Dictionary with mapping `command` -> `its aliases`.
    """
    return defaultdict(list, {"build": ["b"], "run": ["r"], "test": ["t"]})


# FIXME: Implement (recursive) alias expansion. Note that aliases from configuration
#        should either be a string, that we can safely `.split(" ")`, or an already splitted string
#        (look at Cargo's example: https://doc.rust-lang.org/cargo/reference/config.html#configuration-format).
#        We should also update the docs.
