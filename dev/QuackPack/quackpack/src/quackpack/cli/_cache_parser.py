import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `cache` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `cache` command.
    """
    parser = argparse.ArgumentParser(prog="cache", description="Manage the cache")
    parser.add_argument("-c", "--clean", action="store_true", help="Clean the cache")
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
    info.console.debug("Implement 'execute()' for 'cache'")
    raise NotImplementedError
