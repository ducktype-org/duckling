import argparse
import logging
import sys
from contextlib import chdir
from os import getenv
from pathlib import Path
from typing import Never

from quackpack.cli import get_early_parser
from quackpack.config.user import Config
from quackpack.console import Console
from quackpack.errors import QuackPackError
from quackpack.global_info import GlobalInfo
from quackpack.logger import get_logger, setup_logger
from quackpack.main import real_main


def logger() -> logging.Logger:
    """
    Get `logging.Logger` for this module.
    """
    return get_logger(__name__)


def main() -> None:
    """
    Quack Pack main entry point.
    """
    setup_logger()
    logger().debug("Starting Quack Pack")
    # TODO: Tweak console settings.
    console = Console()

    def handle_verbose_and_quiet_log_levels(known_args: argparse.Namespace) -> None:
        if known_args.quiet:
            console.quiet = True
        elif known_args.verbose:
            console.set_verbose()

    # NOTE: We need to remove executable name (sys.argv[0]) from arguments, because of how parser works...
    qp_args, script_args = split_args_for_script(sys.argv[1:])
    logger().debug(f"Quack Pack arguments are {qp_args}, extra script arguments are {script_args}")
    # We execute script, but arguments are missing.
    if script_args is not None and len(script_args) == 0:
        console.warn("Empty script arguments are not supported")
        return None

    # NOTE: Ignore `--help`, because we don't want to bail fast.
    #       real_main handles `--help` with full parser.
    parser = get_early_parser(add_help=False)
    try:
        known_args, _ = parser.parse_known_args()
    except argparse.ArgumentError as e:
        _handle_argparse_exception(console, e)
    except Exception as e:
        _handle_arbitrary_exception(console, e)

    handle_verbose_and_quiet_log_levels(known_args)

    try:
        with chdir(known_args.directory):
            logger().debug(f"Changing current working directory to {known_args.directory}")
            config = Config.load_from_file()
            info = GlobalInfo(user_config=config, qp_args=qp_args, script_args=script_args, console=console)
            sys.exit(real_main(info))
    except argparse.ArgumentError as e:
        _handle_argparse_exception(console, e)
    except FileNotFoundError as e:
        _handle_eexist(console, e)
    except NotADirectoryError as e:
        _handle_enotdir(console, e)
    except QuackPackError as e:
        _handle_quackpack_error(console, e)
    except Exception as e:
        _handle_arbitrary_exception(console, e)


def split_args_for_script(arguments: list[str]) -> tuple[list[str], list[str] | None]:
    """
    Split `arguments` into a program and a script arguments.
    ----
    Args:
    - `arguments`: list of arguments to be splitted at first double dash.
    ----
    Returns:
    - `tuple[list[str], list[str] | None]` - `tuple` with `list` of program arguments and optional script arguments.
    """
    # We split cli arguments at first double dash, to distinguish between Quack Pack arguments,
    # and extra script arguments.
    double_dash_idx = [*arguments, "--"].index("--")
    return (
        (arguments, None)
        if double_dash_idx == len(arguments)
        else (arguments[:double_dash_idx], arguments[double_dash_idx + 1 :])
    )


def _handle_argparse_exception(console: Console, e: argparse.ArgumentError, code: int = 1) -> Never:
    """
    Print nicely formatted `argparse.ArgumentError` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `argparse.ArgumentError`.
    - `code`: exit code, defaults to 1.
    """
    console.error("in parser:")
    console.print(f"{' ' * 4}{e.message}", style="bold yellow")
    sys.exit(code)


def _handle_enotdir(console: Console, e: NotADirectoryError, code: int = 1) -> Never:
    """
    Print nicely formatted `NotADirectoryError` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `NotADirectoryError`.
    - `code`: exit code, defaults to 1.
    """
    if e.filename is None:
        console.critical("Not a directory error, but filename is missing")
    else:
        console.error(f"File '{Path(e.filename)}' is not a directory.")
    sys.exit(code)


def _handle_eexist(console: Console, e: FileNotFoundError, code: int = 1) -> Never:
    """
    Print nicely formatted `FileNotFoundError` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `FileNotFoundError`.
    - `code`: exit code, defaults to 1.
    """
    if e.filename is None:
        console.critical("File not found error, but filename is missing")
    else:
        console.error(f"File '{Path(e.filename)}' does not exist.")
    sys.exit(code)


def _handle_quackpack_error(console: Console, e: QuackPackError, code: int = 1) -> Never:
    """
    Print nicely formatted `QuackPackError` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `QuackPackError`.
    - `code`: exit code, defaults to 1.
    """
    console.error(str(e))
    sys.exit(code)


def _handle_arbitrary_exception(console: Console, e: Exception, code: int = 1) -> Never:
    """
    Print nicely formatted `Exception` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `Exception`.
    - `code`: exit code, defaults to 1.
    """
    error_message = f"with message [bold yellow]{e!s}" if str(e) != "" else ""
    console.critical(f"Unexpected error of type [bold yellow]'{type(e).__name__}'[/] {error_message}")
    debug_level = getenv("QP_EXCEPTION_DEBUG")
    if debug_level in ("1", "2"):
        console.print_exception(show_locals=debug_level == "2")
    sys.exit(code)
