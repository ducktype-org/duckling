import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `unpublish` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `unpublish` command.
    """
    parser = argparse.ArgumentParser(prog="unpublish", description="Unpublish a package from the registry")
    parser.add_argument("package", type=str, help="Package name")
    return parser
