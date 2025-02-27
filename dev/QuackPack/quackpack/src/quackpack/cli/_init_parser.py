import argparse
from pathlib import Path

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `init` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `init` command.
    """
    parser = argparse.ArgumentParser(prog="init", description="Initialize a new project")
    parser.add_argument(
        "path", nargs="?", default=Path.cwd(), type=Path, help="Path to the project's directory"
    )
    return parser


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
    info.console.debug("Implement 'execute()' for 'init'")
    raise NotImplementedError
