import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `info` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `info` command.
    """
    parser = argparse.ArgumentParser(prog="info", description="Get a package informations")
    parser.add_argument("package", type=str, help="Package name")
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
    info.console.debug("Implement 'execute()' for 'info'")
    raise NotImplementedError
