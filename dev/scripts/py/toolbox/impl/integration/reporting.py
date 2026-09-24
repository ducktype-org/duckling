import pathlib
import sys
import threading
from dataclasses import astuple, dataclass
from functools import partial

import click

from .progress import PROGRESS
from ..helpers import (
    Timeout,
    WrongExitcode,
    click_log,
    log_good,
    log_info,
    log_warning,
)

# The directory every path in a run is rooted at. It is identical for every
# test, so printing it on ~300 section headers only costs width.
SUITE_ROOT = "integration_tests"

# Case lines sit under their test's header.
CASE_INDENT = "  "



def print_success(msg, file=sys.stdout):
    with PROGRESS.paused():
        log_good(msg, file=file)


def print_failure(msg, file=sys.stdout):
    with PROGRESS.paused():
        click_log("FAIL", msg, fg="red", file=file)


def print_info(msg: str, file=sys.stdout):
    """`log_info`, with the line taken down first."""
    with PROGRESS.paused():
        log_info(msg, file=file)


def print_warning(msg: str, file=sys.stdout):
    """`log_warning`, with the line taken down first."""
    with PROGRESS.paused():
        log_warning(msg, file=file)


def short_path(path: str) -> str:
    """
    Drops the suite root from a path, which every test in a run shares.
    """
    prefix = SUITE_ROOT + "/"
    if path.startswith(prefix):
        return path[len(prefix):]
    return path


def print_section(path: str):
    """
    Prints a test's header, which marks where one test ends and the next begins.
    """
    with PROGRESS.paused():
        click_log("ITEST", short_path(path), fg="bright_blue", bold=True)


def note_preparing(path: str):
    """
    Says what the run is doing while a node's `PreNode` runs.
    """
    PROGRESS.set(
        "preparing the run" if path == SUITE_ROOT else f"preparing {short_path(path)}"
    )


def note_test_finished(path: str, failures: int = 0):
    """
    Counts one finished test on the line, however it finished.
    """
    PROGRESS.advance(short_path(path), failures)


def first_line(msg: str) -> str:
    """
    Keeps a failure to one console line; the log keeps the full text.
    """
    lines = msg.strip().splitlines()
    return lines[0] if lines else msg


def describe_exit_status(exit_status) -> str:
    """
    A short phrase for a command that failed, e.g. `exit code 3, expected 0`.
    `BashCommandError.reason_string` is a full sentence and stays as it is.
    """
    match exit_status:
        case WrongExitcode(expected, got):
            return f"exit code {got}, expected {expected}"
        case Timeout(timeout):
            return f"timed out after {timeout}s"
    return "command failed"


def _case_line(prefix: str, msg: str, fg: str, file=sys.stdout):
    with PROGRESS.paused():
        click.echo(
            CASE_INDENT + click.style(f"[{prefix}]: {msg}", fg=fg), color=True, file=file
        )


def print_case_passed(msg: str, file=sys.stdout):
    _case_line("GOOD", msg, "green", file)


def print_case_failed(msg: str, file=sys.stdout):
    _case_line("FAIL", msg, "red", file)


def print_case_skipped(msg: str, file=sys.stdout):
    _case_line("SKIP", msg, "bright_black", file)


# How each outcome prints, the colour it lends a folded test, and its counter.
CASE_OUTCOMES = {
    "passed": (print_case_passed, "green", "succeeded"),
    "disabled": (print_case_skipped, "bright_black", "disabled"),
    "failed": (print_case_failed, "red", "failed"),
}

# Keyed by the printer itself: a case line emitted any other way is not folded.
CASE_PRINTERS = {printer: status for status, (printer, _, _) in CASE_OUTCOMES.items()}


def _print_folded_section(path: str, status: str, msg: str):
    with PROGRESS.paused():
        click_log(
            "ITEST", f"{short_path(path)} — {msg}", fg=CASE_OUTCOMES[status][1], bold=True
        )


def format_tally(stats: "TestStatistics") -> str:
    """
    A test's case outcomes, e.g. `7 passed, 4 disabled, 1 failed`.
    """
    counted = (
        (status, len(getattr(stats, counter)))
        for status, (_, _, counter) in CASE_OUTCOMES.items()
    )
    return ", ".join(f"{count} {status}" for status, count in counted if count)


def format_not_run(count: int) -> str:
    """
    The tally for a test whose cases never ran, which is all it can say.
    """
    return f"{count} not run"


def section_for(path: str, section: list, single_case: bool, tally: str = "") -> list:
    """
    Wraps a test's output section in its header. A test with a single case is
    printed as one line instead, since the header and the case line would repeat
    each other, and a section with nothing in it gets no header either.
    """
    if single_case and len(section) == 1 and section[0][0] in CASE_PRINTERS:
        print_fn, msg = section[0]
        status = CASE_PRINTERS[print_fn]
        if status == "disabled":
            msg = f"{msg} (skipped)"
        return [(partial(_print_folded_section, path, status), msg)]
    if not section:
        return []
    label = short_path(path)
    if tally:
        label = f"{label} ({tally})"
    return [(print_section, label), *section]


_WRITE_LOG_LOCK = threading.Lock()


def write_log(msg, log_file):
    """
    Dumps a `msg` message into a log file `log_file`.
    """
    with _WRITE_LOG_LOCK:
        with open(log_file, "a") as f:
            print(">>>" + msg + f"{'-' * 50}", file=f)


def log_info_if_needed(msg: str, dry: bool, verbose: bool):
    if dry or verbose:
        print_info(msg)


class CaseLog:
    """
    Collects the console lines and log-file chunks of a single test case.
    In immediate mode (sequential runs) everything is emitted right away;
    otherwise the output is buffered: `flush` writes the log-file chunks
    and the console lines stay in `console` for the test's output section.
    """

    def __init__(
        self, log_file: pathlib.Path, immediate: bool = True, quiet: bool = False
    ):
        self.log_file = log_file
        self.immediate = immediate
        self.quiet = quiet
        self.console = []
        self.chunks = []

    def emit(self, print_fn, msg: str):
        if self.immediate:
            print_fn(msg)
        else:
            self.console.append((print_fn, msg))

    def emit_non_failure(self, print_fn, msg: str):
        """
        Emits a case that did not fail; `--quiet` drops these, and failures are
        printed either way.
        """
        if not self.quiet:
            self.emit(print_fn, msg)

    def info_if_needed(self, msg: str, dry: bool, verbose: bool):
        if dry or verbose:
            self.emit(print_info, msg)

    def log(self, msg: str):
        if self.immediate:
            write_log(msg, self.log_file)
        else:
            self.chunks.append(msg)

    def flush(self):
        for chunk in self.chunks:
            write_log(chunk, self.log_file)
        self.chunks = []


class CompletionOutput:
    """
    Prints every submitted test section as soon as it arrives (completion
    order); the lock keeps concurrently finishing sections atomic.
    """

    def __init__(self):
        self.lock = threading.Lock()

    def register(self, path: str):
        pass

    def submit(self, path: str, section: list):
        with self.lock:
            for print_fn, msg in section:
                print_fn(msg)

    def emit_now(self, section: list):
        self.submit("", section)

    def drain(self):
        pass


class OrderedOutput:
    """
    Prints buffered test sections in the registration (tree) order, no
    matter in which order the concurrently running tests finish. Every
    test is registered up front; a finished test submits its section
    exactly once, which is printed once all sections before it have been
    printed.
    """

    def __init__(self):
        self.lock = threading.Lock()
        self.order: dict[str, int] = {}
        self.sections: dict[int, list] = {}
        self.next_to_print = 0

    def register(self, path: str):
        self.order[path] = len(self.order)

    def submit(self, path: str, section: list):
        with self.lock:
            self.sections[self.order[path]] = section
            while self.next_to_print in self.sections:
                for print_fn, msg in self.sections.pop(self.next_to_print):
                    print_fn(msg)
                self.next_to_print += 1

    def emit_now(self, section: list):
        """
        Prints outside of the registered order; for rare out-of-band
        messages (e.g. PreNode/PostNode failures) that belong to no
        test section.
        """
        with self.lock:
            for print_fn, msg in section:
                print_fn(msg)

    def drain(self):
        """
        Prints whatever is left in order; used at the end of the run,
        when some registered tests never submitted (e.g. fail-fast).
        """
        with self.lock:
            for seq in sorted(self.sections):
                for print_fn, msg in self.sections.pop(seq):
                    print_fn(msg)


@dataclass
class Success:
    pass


@dataclass
class OutputMismatch:
    error: str


@dataclass
class Disabled:
    pass


@dataclass
class TestStatistics:
    """
    Describes the number of ran and succeeded, failed, disabled test cases.
    """

    succeeded: list[str]
    failed: list[str]
    disabled: list[str]

    def __add__(self, other):
        return TestStatistics(
            self.succeeded + other.succeeded,
            self.failed + other.failed,
            self.disabled + other.disabled,
        )

    def iadd(self, other):
        self.failed += other.failed
        self.succeeded += other.succeeded
        self.disabled += other.disabled

    def __iter__(self):
        return iter(astuple(self))
