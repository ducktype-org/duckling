import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `logout` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `logout` command.
    """
    parser = argparse.ArgumentParser(prog="logout", description="Logout from the registry")
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
    info.console.debug("Implement 'execute()' for 'logout'")
    raise NotImplementedError
