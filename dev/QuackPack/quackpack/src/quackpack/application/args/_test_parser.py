import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `test` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `test` command.
    """
    parser = argparse.ArgumentParser(prog="test", description="Test a current project")
    return parser
