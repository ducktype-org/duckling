# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""Parsing of `duckc repl --framed` output into kernel events.

Markers (each on its own line):

* ``\\x1e@duck-end`` ends one input sent to the REPL.
* ``\\x1e@duck-ready`` is printed on stdout and stderr when the REPL waits for input.
* ``\\x1e@duck-result <value>`` holds an expression's result.
* ``\\x1e@duck-display begin <mime>`` ... ``\\x1e@duck-display end`` wraps one rich output
  part printed by the program. Parts printed back to back form one bundle (one notebook output
  with several MIME types).
"""

from __future__ import annotations

from dataclasses import dataclass, field

END_OF_INPUT = "\x1e@duck-end"
READY = "\x1e@duck-ready"
RESULT = "\x1e@duck-result "
DISPLAY_BEGIN = "\x1e@duck-display begin "
DISPLAY_END = "\x1e@duck-display end"


@dataclass
class Stream:
    name: str  # "stdout" or "stderr"
    text: str


@dataclass
class Display:
    data: dict[str, str] = field(default_factory=dict)


@dataclass
class Result:
    text: str


@dataclass
class Ready:
    pass


@dataclass
class Exited:
    code: int | None


Event = Stream | Display | Result | Ready | Exited


def frame_input(code: str) -> str:
    """Wrap one cell (or command) so the REPL reads it as one input."""
    return code.rstrip("\n") + "\n" + END_OF_INPUT + "\n"


class StdoutParser:
    """Turns stdout lines (without their newline) into events."""

    def __init__(self) -> None:
        self._bundle: dict[str, str] = {}
        self._part_mime: str | None = None
        self._part_lines: list[str] = []

    def feed_line(self, line: str) -> list[Event]:
        if self._part_mime is not None:
            if line == DISPLAY_END:
                self._bundle[self._part_mime] = "\n".join(self._part_lines)
                self._part_mime = None
                self._part_lines = []
            else:
                self._part_lines.append(line)
            return []

        if line.endswith(READY):
            # A marker may follow output that did not end its line.
            prefix = line[: -len(READY)]
            events = self.feed_line(prefix) if prefix else []
            return events + self.flush() + [Ready()]

        if line.startswith(DISPLAY_BEGIN):
            self._part_mime = line[len(DISPLAY_BEGIN) :].strip()
            return []

        events = self.flush()
        if line.startswith(RESULT):
            events.append(Result(line[len(RESULT) :]))
        else:
            events.append(Stream("stdout", line + "\n"))
        return events

    def flush(self) -> list[Event]:
        """Emit the pending display bundle, if any.

        An unterminated part is emitted as plain text so output is never lost.
        """
        events: list[Event] = []
        if self._part_mime is not None:
            text = "\n".join([DISPLAY_BEGIN + self._part_mime, *self._part_lines])
            events.append(Stream("stdout", text + "\n"))
            self._part_mime = None
            self._part_lines = []
        if self._bundle:
            events.insert(0, Display(self._bundle))
            self._bundle = {}
        return events
