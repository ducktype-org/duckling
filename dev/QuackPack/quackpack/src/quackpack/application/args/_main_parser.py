import argparse
from pathlib import Path

from rich_argparse import RichHelpFormatter


def get_early_parser() -> argparse.ArgumentParser:
    """
    Get early (unfinished) main Quack Pack argument parser.
    ----
    Returns:
    - `argparse.ArgumentParser`: Main Quack Pack parser, but without any subcommands.
    """
    parser = argparse.ArgumentParser(
        description="Duckling's package manager", formatter_class=RichHelpFormatter
    )
    parser.add_argument("-V", "--version", action="version", version="%(prog)s v0.1.0")
    parser.add_argument(
        "-C",
        "--directory",
        nargs="?",
        default=Path.cwd(),
        type=Path,
        help="Change to DIRECTORY before doing any actions",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-v", "--verbose", action="store_true", help="Use verbose output")
    group.add_argument("-q", "--quiet", action="store_true", help="Suppress all output")
    return parser
