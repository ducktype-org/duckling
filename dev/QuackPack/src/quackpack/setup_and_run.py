import argparse
import asyncio
import os
import sys
from pathlib import Path
from typing import Never

from quackpack.cli import setup_parser
from quackpack.config.user import Config
from quackpack.run import run
from quackpack.signals import ForcedSignal, SignalInterrupt
from quackpack.util.console import Console
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger, setup_logger

if os.name == "posix":
    try:
        import uvloop

        asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
    except ImportError:
        pass

logger = get_logger(__name__)


def setup_and_run() -> int | None:
    """
    Quack Pack main entry point.
    """
    setup_logger()
    logger.debug("Starting Quack Pack")
    # TODO: Tweak console settings.
    setup_parser()
    console = Console()
    error_console = Console(stderr=True)

    # NOTE: We need to remove executable name (sys.argv[0]) from arguments, because of how parser works...
    qp_args, script_args = split_args_for_script(sys.argv[1:])
    logger.debug(f"Quack Pack arguments are {qp_args}, extra script arguments are {script_args}")
    # We execute script, but arguments are missing.
    if script_args is not None and not script_args:
        console.warn("Empty script arguments are not supported")
        return 1

    try:
        config = Config.load_from_file(console)
        ctx = GlobalContext(
            configuration=config,
            qp_args=qp_args,
            script_args=script_args,
            console=console,
            error_console=error_console,
        )
        return run(ctx)
    except argparse.ArgumentError as e:
        _handle_argparse_exception(error_console, e)
    except FileNotFoundError as e:
        _handle_eexist(error_console, e)
    except NotADirectoryError as e:
        _handle_enotdir(error_console, e)
    except QuackPackError as e:
        _handle_quackpack_error(error_console, e)
    except ForcedSignal as e:
        _handle_forced_signal(error_console, e)
    except SignalInterrupt:
        raise
    except Exception as e:
        _handle_arbitrary_exception(error_console, e)


def split_args_for_script(arguments: list[str]) -> tuple[list[str], list[str] | None]:
    """
    Split `arguments` into a program and a script arguments.
    ----
    Args:
    - `arguments`: list of arguments to be split at first double dash.
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


def _handle_forced_signal(console: Console, _e: ForcedSignal, code: int = 1) -> Never:
    """
    Print nicely formatted `ForcedSignal` and exit.
    ----
    Args:
    - `console`: `rich.console.Console` used for printing.
    - `e`: caught `ForcedSignal`.
    - `code`: exit code, defaults to 1.
    """
    console.print_exception(show_locals=False)
    console.critical(
        "Greetings, you have encountered a critical bug.",
        "",
        "We would be very thankful, were You to contact us with some details about what happened, by filing an issue to REPO_LINK.",
        "",
        "Best regards, QuackPack developers",
        sep="\n",
    )
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
    debug_level = os.getenv("QP_EXCEPTION_DEBUG")
    if debug_level in ("1", "2"):
        console.print_exception(show_locals=debug_level == "2")
    sys.exit(code)
