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


def print_neutral(msg, file=sys.stdout):
    with PROGRESS.paused():
        click_log("INFO", msg, fg="white", file=file)


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
    Prints a test's header. It gets a tag of its own rather than `[INFO]`
    because it marks where one test ends and the next begins.
    """
    with PROGRESS.paused():
        click_log("ITEST", short_path(path), fg="bright_blue", bold=True)


def first_line(msg: str) -> str:
    """
    Keeps a failure to one console line; the log keeps the full text.
    """
    lines = msg.strip().splitlines()
    return lines[0] if lines else msg


def describe_exit_status(exit_status) -> str:
    """
    A short phrase for a command that failed, e.g. `exit code 3, expected 0`.
    `BashCommandError.reason_string` is a full sentence and is shared with the
    cpp linter, so it is left as it is.
    """
    match exit_status:
        case WrongExitcode(expected, got):
            return f"exit code {got}, expected {expected}"
        case Timeout(timeout):
            return f"timed out after {timeout}s"
    return "command failed"


def _case_line(prefix: str, msg: str, fg: str):
    with PROGRESS.paused():
        click.echo(CASE_INDENT + click.style(f"[{prefix}]: {msg}", fg=fg), color=True)


def print_case_passed(msg: str):
    _case_line("GOOD", msg, "green")


def print_case_failed(msg: str):
    _case_line("FAIL", msg, "red")


def print_case_skipped(msg: str):
    _case_line("SKIP", msg, "bright_black")


# The colour of a single-case test's header, taken from its only case.
SECTION_STYLES = {"passed": "green", "failed": "red", "skipped": "bright_black"}

# The outcome each case printer reports. Keyed by the printer object itself, so
# a case line emitted through anything else - a `partial`, a lambda - would
# quietly stop folding its single-case test into one line.
CASE_STATUSES = {
    print_case_passed: "passed",
    print_case_failed: "failed",
    print_case_skipped: "skipped",
}


def _print_folded_section(path: str, status: str, msg: str):
    with PROGRESS.paused():
        click_log(
            "ITEST", f"{short_path(path)} — {msg}", fg=SECTION_STYLES[status], bold=True
        )


def format_tally(stats: "TestStatistics") -> str:
    """
    A test's case outcomes, e.g. `7 passed, 4 disabled, 1 failed`. Under
    `--quiet` the case lines are gone, so this is the only per-test record of
    how the test went; outcomes that did not occur are left out, and a test
    whose cases never ran tallies to nothing.
    """
    counts = (
        (len(stats.succeeded), "passed"),
        (len(stats.disabled), "disabled"),
        (len(stats.failed), "failed"),
    )
    return ", ".join(f"{count} {name}" for count, name in counts if count)


def section_for(path: str, section: list, single_case: bool, tally: str = "") -> list:
    """
    Wraps a test's output section in its header.

    Most tests in the suite have exactly one case, so the header and the case
    line would repeat each other's only content; those are printed as one line.
    Tests with more to say keep the header and their indented lines. A section
    with nothing in it gets no header either, so a test whose cases all passed
    under `--quiet` - or whose cases were all cut short by fail-fast - leaves no
    trace rather than a header with nothing under it.
    """
    if single_case and len(section) == 1 and section[0][0] in CASE_STATUSES:
        print_fn, msg = section[0]
        status = CASE_STATUSES[print_fn]
        if status == "skipped":
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
        Emits a case that did not fail. Passing and skipped cases are the bulk
        of a run's output and say the least, so `--quiet` drops both; failures
        are printed either way.
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
