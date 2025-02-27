import argparse
from pathlib import Path

from rich_argparse import RichHelpFormatter


def get_early_parser(*, add_help: bool = True) -> argparse.ArgumentParser:
    """
    Get early (unfinished) main Quack Pack argument parser.
    ----
    Args:
    - `add_help`: should returned parser contain automatically generated help message.
    ----
    Returns:
    - `argparse.ArgumentParser`: Main Quack Pack parser, but without any subcommands.
    """
    RichHelpFormatter.styles["argparse.args"] = "bold cyan"
    RichHelpFormatter.styles["argparse.groups"] = "bold #5fd7ff"
    RichHelpFormatter.styles["argparse.metavar"] = "deep_sky_blue1"
    RichHelpFormatter.styles["argparse.prog"] = "bold cyan"
    RichHelpFormatter.usage_markup = True
    parser = argparse.ArgumentParser(
        description="Duckling's package manager",
        formatter_class=RichHelpFormatter,
        exit_on_error=False,
        add_help=add_help,
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
