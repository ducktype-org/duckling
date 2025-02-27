import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `unpublish` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `unpublish` command.
    """
    parser = argparse.ArgumentParser(prog="unpublish", description="Unpublish a package from the registry")
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
    info.console.debug("Implement 'execute()' for 'unpublish'")
    raise NotImplementedError
