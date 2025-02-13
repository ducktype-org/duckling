import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `cache` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `cache` command.
    """
    parser = argparse.ArgumentParser(prog="cache", description="Manage the cache")
    parser.add_argument("-c", "--clean", action="store_true", help="Clean the cache")
    return parser
