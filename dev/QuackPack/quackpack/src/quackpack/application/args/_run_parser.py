import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `run` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `run` command.
    """
    parser = argparse.ArgumentParser(prog="run", description="Build a current project and run it")
    return parser
