import argparse
import asyncio
import os
import sys
from itertools import chain
from pathlib import Path
from typing import Never

from pydantic_core import ErrorDetails, ValidationError

from quackpack.cli import setup_parser
from quackpack.config.user import Configuration
from quackpack.run import run
from quackpack.signals import ForcedSignal, SignalInterrupt
from quackpack.util.console import Console
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger, setup_logger

asyncio.set_event_loop_policy(asyncio.DefaultEventLoopPolicy())
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
    setup_parser()
    console = Console()
    error_console = Console(stderr=True)

    try:
        configuration = Configuration.load_from_file(console)
        ctx = GlobalContext(configuration=configuration, console=console, error_console=error_console)
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
    except ValidationError as e:
        _handle_pydantic_error(error_console, e)
    except Exception as e:
        _handle_arbitrary_exception(error_console, e)


def _handle_argparse_exception(console: Console, e: argparse.ArgumentError, code: int = 1) -> Never:
    """
    Print nicely formatted `argparse.ArgumentError` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `argparse.ArgumentError`.
    - `code`: exit code, defaults to 1.
    """
    console.error("in parser:")
    console.print(f"{' ' * 4}{e.message}", style="bold yellow")
    for note in getattr(e, "__notes__", []):
        console.print(note)
    sys.exit(code)


def _handle_enotdir(console: Console, e: NotADirectoryError, code: int = 1) -> Never:
    """
    Print nicely formatted `NotADirectoryError` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `NotADirectoryError`.
    - `code`: exit code, defaults to 1.
    """
    if e.filename is None:
        console.critical("Not a directory error, but filename is missing")
    else:
        console.error(f"File '{Path(e.filename)}' is not a directory.")
    for note in getattr(e, "__notes__", []):
        console.print(note)
    sys.exit(code)


def _handle_eexist(console: Console, e: FileNotFoundError, code: int = 1) -> Never:
    """
    Print nicely formatted `FileNotFoundError` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `FileNotFoundError`.
    - `code`: exit code, defaults to 1.
    """
    if e.filename is None:
        console.critical("File not found error, but filename is missing")
    else:
        console.error(f"File '{Path(e.filename)}' does not exist.")
    for note in getattr(e, "__notes__", []):
        console.print(note)
    sys.exit(code)


def _handle_quackpack_error(console: Console, e: QuackPackError, code: int = 1) -> Never:
    """
    Print nicely formatted `QuackPackError` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `QuackPackError`.
    - `code`: exit code, defaults to 1.
    """
    console.error(e)
    for note in getattr(e, "__notes__", []):
        console.print(note)
    sys.exit(code)


def _handle_forced_signal(console: Console, _e: ForcedSignal, code: int = 1) -> Never:
    """
    Print nicely formatted `ForcedSignal` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `ForcedSignal`.
    - `code`: exit code, defaults to 1.
    """
    console.print_exception(show_locals=False)
    console.critical(
        """Greetings, you have encountered a critical bug.

        We would be very thankful, were You to contact us with some details about what happened, by filing an issue to REPO_LINK.

        Best regards, QuackPack developers"""
    )
    sys.exit(code)


def _handle_pydantic_error(console: Console, e: ValidationError, code: int = 1) -> Never:
    """
    Print nicely formatted `ValidationError` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `ValidationError`.
    - `code`: exit code, defaults to 1.
    """
    for error in e.errors():
        _print_single_pydantic_error(console, error, e.title)
    for note in getattr(e, "__notes__", []):
        console.print(note)
    sys.exit(code)


def _print_single_pydantic_error(console: Console, e: ErrorDetails, field: str) -> None:
    """
    Print nicely formatted `ErrorDetails`.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: `ErrorDetails`.
    """
    field = field.lower()
    location = ".".join(str(x) for x in chain((field,), e["loc"]))
    message = str(e["ctx"]["error"]) if "error" in e.get("ctx", {}) else e["msg"]  # pyright: ignore[reportTypedDictNotRequiredAccess], we are using `get` with empty dictionary as a fallback, hence if we are in first branch, then `"ctx" in e` must be true.
    console.error(f"in [bold blue]{location}[/]: [bold cyan]{message}[/]")


def _handle_arbitrary_exception(console: Console, e: Exception, code: int = 1) -> Never:
    """
    Print nicely formatted `Exception` and exit.
    ----
    Args:
    - `console`: `Console` used for printing.
    - `e`: caught `Exception`.
    - `code`: exit code, defaults to 1.
    """
    error_message = f"with message [bold yellow]{e!s}" if str(e) != "" else ""
    console.critical(f"Unexpected error of type [bold yellow]'{type(e).__name__}'[/] {error_message}")
    for note in getattr(e, "__notes__", []):
        console.print(note)
    debug_level = os.getenv("QP_EXCEPTION_DEBUG")
    if debug_level in ("1", "2"):
        console.print_exception(show_locals=debug_level == "2")
    sys.exit(code)
