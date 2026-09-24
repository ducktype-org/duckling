"""
The run's progress line: one line, rewritten in place, for `--quiet` runs
(`itest --quiet`, or `pr-validate --quiet`, which passes the flag on).

It needs a terminal: stdout when it is one, otherwise `/dev/tty`, so
`itest | tee log` keeps the line out of the log. Writers that print on their own
go through `PausedStream`, or they overwrite it.
"""

import atexit
import os
import sys
import threading
import time

# Back to the start of the line, and erase what was on it.
_CLEAR = "\r\x1b[K"


def _elide_middle(text: str, room: int) -> str:
    """Shortens `text` to `room` columns, keeping both ends."""
    if room <= 0:
        return ""
    if len(text) <= room:
        return text
    if room <= 3:
        return text[:room]
    kept = room - 1
    front = (kept + 1) // 2
    return f"{text[:front]}…{text[front - kept:]}"


class ProgressLine:
    """The line: what to count, where to draw it, and how not to collide with
    everything else written to the same terminal."""

    # How long a reader relaying another stream gets before the line goes back
    # up: enough to cover a pipe write, short enough not to swallow redraws.
    REDRAW_GRACE = 0.05

    def __init__(self):
        # With no terminal the line is inert, so callers need no guard of their own.
        self.lock = threading.RLock()
        self.line = None
        self.owned_line = None
        self.total = 0
        self.done = 0
        self.failures = 0
        self.label = ""
        self.drawn = False
        self.redraw_after = 0.0

    def reset(self, total: int):
        """Starts the count for a run of `total` tests, with no terminal attached."""
        with self.lock:
            self._release_line()
            self.total = total
            self.done = 0
            self.failures = 0
            self.label = ""
            self.drawn = False
            self.redraw_after = 0.0

    def attach(self):
        """Finds a terminal to draw on. Without one every draw is a no-op, so this
        is what decides whether the run has a line at all."""
        with self.lock:
            self._acquire_line()

    def _acquire_line(self):
        """Picks a terminal to draw on, if the process has one."""
        if sys.stdout.isatty():
            self.line = sys.stdout
            return
        try:
            tty = open("/dev/tty", "w")
        except OSError:
            return
        if tty.isatty():
            self.line = tty
            self.owned_line = tty
        else:
            tty.close()

    def _release_line(self):
        if self.owned_line is not None:
            self.owned_line.close()
            self.owned_line = None
        self.line = None

    def advance(self, label: str, failures: int = 0):
        """Counts one finished test."""
        with self.lock:
            self.done += 1
            self.failures += failures
            self.label = label
            self._draw()

    def set(self, label: str):
        """Says what the run is doing when there is no count to move yet."""
        with self.lock:
            self.label = label
            self._draw()

    def _render(self, width: int | None = None) -> str:
        """The line as it should read, trimmed to `width`."""
        head = f"[{self.done}/{self.total}]"
        suffix = f" · {self.failures} failed" if self.failures else ""
        label = self.label
        if width is not None:
            label = _elide_middle(label, width - len(head) - 1 - len(suffix))
        text = f"{head} {label}{suffix}" if label else f"{head}{suffix}"
        if width is not None and len(text) > width:
            text = text[:width]
        return text

    def _width(self) -> int | None:
        """How wide the line may be. One column spare: filling the last one
        leaves the cursor mid-wrap, which some terminals resolve at the wrong
        moment."""
        try:
            return os.get_terminal_size(self.line.fileno()).columns - 1
        except (OSError, ValueError, AttributeError):
            return None

    def _emit(self, text: str) -> bool:
        """Writes to the line's terminal. The line is decoration: a terminal that
        has gone away must cost the line, never the run."""
        try:
            self.line.write(text)
            self.line.flush()
            return True
        except (OSError, ValueError):
            self.drawn = False
            self._release_line()
            return False

    def _draw(self):
        # A filter that matched nothing must not leave a `[0/0]` line behind.
        if self.line is None or not self.total:
            return
        if time.monotonic() < self.redraw_after:
            return
        if self._emit(_CLEAR + self._render(self._width())):
            self.drawn = True

    def paused(self):
        """Takes the line down while other output is written."""
        return _ProgressPause(self)

    def close(self):
        """Removes the line for good."""
        with self.lock:
            if self.drawn:
                self._emit(_CLEAR)
                self.drawn = False
            self._release_line()


class _ProgressPause:
    """Takes the line down around a write, and puts it back once that write has
    had time to be relayed."""

    def __init__(self, progress: ProgressLine):
        self.progress = progress
        self.was_drawn = False

    def __enter__(self):
        self.progress.lock.acquire()
        # The lock is taken by hand, so it has to be handed back by hand: one
        # left held would block every other printer and hang the run.
        try:
            if self.progress.drawn:
                self.progress._emit(_CLEAR)
                self.progress.drawn = False
                self.was_drawn = True
        except BaseException:
            self.progress.lock.release()
            raise

    def __exit__(self, *exc):
        try:
            if self.progress.line is sys.stdout:
                # Line and output share a stream, so the order is ours.
                if self.was_drawn:
                    self.progress._draw()
            elif self.progress.line is not None:
                # The line is on another terminal while something relays this
                # output to it: wait for the relay, whether or not the line was
                # up for this particular write.
                self.progress.redraw_after = (
                    time.monotonic() + ProgressLine.REDRAW_GRACE
                )
        finally:
            self.progress.lock.release()


class PausedStream:
    """A stream that takes the line down around every write. For output handed to
    something which prints on its own - the command echo `exec_bash_command`
    emits under `--dry`/`--verbose` - and would otherwise overwrite the line."""

    def __init__(self, stream):
        self.stream = stream

    def write(self, text):
        with PROGRESS.paused():
            return self.stream.write(text)

    def flush(self):
        return self.stream.flush()

    def isatty(self):
        return self.stream.isatty()


PROGRESS = ProgressLine()

# An interrupted run must not leave the line on the terminal.
atexit.register(PROGRESS.close)
