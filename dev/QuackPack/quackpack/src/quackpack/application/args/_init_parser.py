import argparse
from pathlib import Path


def get_parser() -> argparse.ArgumentParser:
    """
    Get the argument parser for `init` command.
    ----
    Returns:
    - `argparse.ArgumentParser`: Parser for `init` command.
    """
    parser = argparse.ArgumentParser(prog="init", description="Initialize a new project")
    parser.add_argument(
        "path", nargs="?", default=Path.cwd(), type=Path, help="Path to the project's directory"
    )
    return parser
