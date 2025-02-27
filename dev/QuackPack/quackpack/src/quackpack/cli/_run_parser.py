import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `run` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `run` command.
    """
    parser = argparse.ArgumentParser(prog="run", description="Build a current project and run it")
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
    info.console.debug("Implement 'execute()' for 'run'")
    raise NotImplementedError
