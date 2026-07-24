import pathlib
import sys
import threading
from dataclasses import astuple, dataclass

from ..helpers import click_log, log_info


def print_success(msg, file=sys.stdout):
    click_log("GOOD", msg, fg="green", file=file)


def print_failure(msg, file=sys.stdout):
    click_log("FAIL", msg, fg="red", file=file)


def print_neutral(msg, file=sys.stdout):
    click_log("INFO", msg, fg="white", file=file)


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
        log_info(msg)


class CaseLog:
    """
    Collects the console lines and log-file chunks of a single test case.
    In immediate mode (sequential runs) everything is emitted right away;
    otherwise the output is buffered: `flush` writes the log-file chunks
    and the console lines stay in `console` for the test's output section.
    """

    def __init__(self, log_file: pathlib.Path, immediate: bool = True):
        self.log_file = log_file
        self.immediate = immediate
        self.console = []
        self.chunks = []

    def emit(self, print_fn, msg: str):
        if self.immediate:
            print_fn(msg)
        else:
            self.console.append((print_fn, msg))

    def info_if_needed(self, msg: str, dry: bool, verbose: bool):
        if dry or verbose:
            self.emit(log_info, msg)

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
