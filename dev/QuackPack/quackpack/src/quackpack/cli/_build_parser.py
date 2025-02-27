import argparse

from quackpack.global_info import GlobalInfo


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `build` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `build` command.
    """
    parser = argparse.ArgumentParser(prog="build", description="Build a current project")
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
    info.console.debug("Implement 'execute()' for 'build'")
    raise NotImplementedError
