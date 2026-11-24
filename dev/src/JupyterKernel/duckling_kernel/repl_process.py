# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""A `duckc repl --framed` child process."""

from __future__ import annotations

import os
import queue
import shlex
import shutil
import subprocess
import threading
from collections.abc import Iterator

from .protocol import READY, Event, Exited, Ready, Stream, StdoutParser, frame_input

_EOF = None


def repl_command() -> list[str]:
    """The command starting the REPL.

    ``DUCKLING_KERNEL_COMMAND`` overrides it; otherwise `duck repl` (from ``DUCK_BIN`` or
    ``PATH``) is used, so cells can import the packages of the notebook's package or the global
    packages. Without `duck`, a bare `duckc repl` gives the standard library only.
    """
    if command := os.environ.get("DUCKLING_KERNEL_COMMAND"):
        return shlex.split(command)
    duck = os.environ.get("DUCK_BIN") or shutil.which("duck")
    if duck:
        return [duck, "repl", "--", "--framed"]
    duckc = shutil.which("duckc")
    if duckc:
        return [duckc, "repl", "--framed"]
    raise FileNotFoundError("neither `duck` nor `duckc` found; set DUCK_BIN or PATH")


class ReplProcess:
    def __init__(self, argv: list[str], cwd: str | None = None) -> None:
        self._argv = argv
        self._cwd = cwd
        self._proc: subprocess.Popen[str] | None = None
        self._lines: queue.Queue[tuple[str, str | None]] = queue.Queue()
        self._pumps: list[threading.Thread] = []

    @property
    def alive(self) -> bool:
        return self._proc is not None and self._proc.poll() is None

    def start(self) -> Iterator[Event]:
        """Start the process; yields startup output until the REPL is ready."""
        self._proc = subprocess.Popen(
            self._argv,
            cwd=self._cwd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        for name, stream in (("stdout", self._proc.stdout), ("stderr", self._proc.stderr)):
            pump = threading.Thread(target=self._pump, args=(name, stream), daemon=True)
            pump.start()
            self._pumps.append(pump)
        yield from self._until_ready()

    def run(self, code: str) -> Iterator[Event]:
        """Send one input; yields its output until the REPL is ready again."""
        assert self._proc is not None and self._proc.stdin is not None
        try:
            self._proc.stdin.write(frame_input(code))
            self._proc.stdin.flush()
        except BrokenPipeError:
            yield Exited(self._proc.wait())
            return
        yield from self._until_ready()

    def terminate(self) -> None:
        if self._proc is None:
            return
        if self._proc.stdin:
            try:
                self._proc.stdin.close()
            except BrokenPipeError:
                pass
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            self._proc.wait()
        for pump in self._pumps:
            pump.join(timeout=5)
        for stream in (self._proc.stdout, self._proc.stderr):
            if stream:
                stream.close()

    def _pump(self, name: str, stream) -> None:
        for line in stream:
            self._lines.put((name, line))
        self._lines.put((name, _EOF))

    def _until_ready(self) -> Iterator[Event]:
        parser = StdoutParser()
        ready = {"stdout": False, "stderr": False}
        while not all(ready.values()):
            name, line = self._lines.get()
            if line is _EOF:
                ready[name] = True
                if name == "stdout":
                    yield from parser.flush()
                    assert self._proc is not None
                    yield Exited(self._proc.wait())
                    return
                continue
            line = line.rstrip("\n")
            if name == "stdout":
                for event in parser.feed_line(line):
                    if isinstance(event, Ready):
                        ready["stdout"] = True
                    else:
                        yield event
            elif line.endswith(READY):
                if prefix := line[: -len(READY)]:
                    yield Stream("stderr", prefix + "\n")
                ready["stderr"] = True
            else:
                yield Stream("stderr", line + "\n")
