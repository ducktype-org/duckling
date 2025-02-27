import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `publish` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `publish` command.
    """
    parser = argparse.ArgumentParser(prog="publish", description="Publish package to the registry")
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
    info.console.debug("Implement 'execute()' for 'publish'")
    raise NotImplementedError
