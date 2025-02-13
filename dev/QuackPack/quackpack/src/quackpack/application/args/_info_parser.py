import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `info` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `info` command.
    """
    parser = argparse.ArgumentParser(prog="info", description="Get a package informations")
    parser.add_argument("package", type=str, help="Package name")
    return parser
