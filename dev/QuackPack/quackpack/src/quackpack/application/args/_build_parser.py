import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `build` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `build` command.
    """
    parser = argparse.ArgumentParser(prog="build", description="Build a current project")
    return parser
