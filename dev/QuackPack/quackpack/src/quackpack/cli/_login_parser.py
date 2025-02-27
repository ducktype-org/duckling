import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `login` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `login` command.
    """
    parser = argparse.ArgumentParser(prog="login", description="Login to the registry")
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
    info.console.debug("Implement 'execute()' for 'login'")
    raise NotImplementedError
