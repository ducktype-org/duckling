import argparse


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `publish` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `publish` command.
    """
    parser = argparse.ArgumentParser(prog="publish", description="Publish package to the registry")
    parser.add_argument("package", type=str, help="Package name")
    return parser
