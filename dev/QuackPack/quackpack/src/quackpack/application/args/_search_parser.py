import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `search` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `search` command.
    """
    parser = argparse.ArgumentParser(prog="search", description="Search for a package in the registry")
    parser.add_argument("package", type=str, help="Package name")
    return parser
