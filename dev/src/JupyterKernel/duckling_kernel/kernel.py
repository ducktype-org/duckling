# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""The ipykernel kernel class."""

from __future__ import annotations

import re

from ipykernel.kernelbase import Kernel

from . import __version__
from .protocol import Display, Event, Exited, Result, Stream
from .repl_process import ReplProcess, repl_command

_ERROR_RE = re.compile(r"\berror\b|panicked", re.IGNORECASE)
_ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
_COMPLETION_END = "End_of_completions"


class DucklingKernel(Kernel):
    implementation = "duckling_kernel"
    implementation_version = __version__
    language = "duckling"
    language_version = "0.1"
    banner = "Duckling"
    language_info = {
        "name": "duckling",
        "mimetype": "text/x-duckling",
        "file_extension": ".dk",
    }

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._repl: ReplProcess | None = None
        self._pending_startup: list[Event] = []

    def _ensure_repl(self) -> None:
        if self._repl is not None and self._repl.alive:
            return
        self._repl = ReplProcess(repl_command())
        startup = list(self._repl.start())
        # Package sync chatter is hidden; startup errors are shown in the next cell.
        if any(_is_failure(event) for event in startup):
            self._pending_startup = startup

    def do_execute(
        self, code, silent, store_history=True, user_expressions=None, allow_stdin=False
    ):
        ok = {
            "status": "ok",
            "execution_count": self.execution_count,
            "payload": [],
            "user_expressions": {},
        }
        try:
            self._ensure_repl()
        except (OSError, FileNotFoundError) as error:
            return self._error("ReplStartError", str(error), silent)

        events = self._pending_startup
        self._pending_startup = []
        failed = False
        result: str | None = None
        if code.strip():
            events = [*events, *self._repl.run(code)]

        for event in _merge_streams(events):
            match event:
                case Stream(name="stderr", text=text):
                    failed = failed or _is_failure(event)
                    self._send_stream("stderr", text, silent)
                case Stream(name=name, text=text):
                    self._send_stream(name, text, silent)
                case Display(data=data):
                    if not silent:
                        self.send_response(
                            self.iopub_socket, "display_data", {"data": data, "metadata": {}}
                        )
                case Result(text=text):
                    result = text
                case Exited(code=exit_code):
                    return self._error(
                        "ReplExited", f"the REPL exited with code {exit_code}", silent
                    )

        if failed:
            return self._error("DucklingError", "see the error output above", silent)

        if result is not None and not silent:
            self.send_response(
                self.iopub_socket,
                "execute_result",
                {
                    "execution_count": self.execution_count,
                    "data": {"text/plain": result},
                    "metadata": {},
                },
            )
        return ok

    def do_complete(self, code, cursor_pos):
        prefix_match = re.search(r"\w+$", code[:cursor_pos])
        prefix = prefix_match.group(0) if prefix_match else ""
        matches: list[str] = []
        if prefix and self._repl is not None and self._repl.alive:
            for event in self._repl.run(f"/complete {prefix}"):
                if isinstance(event, Stream) and event.name == "stdout":
                    word = event.text.strip()
                    if word and word != _COMPLETION_END and word not in matches:
                        matches.append(word)
        return {
            "matches": matches,
            "cursor_start": cursor_pos - len(prefix),
            "cursor_end": cursor_pos,
            "metadata": {},
            "status": "ok",
        }

    def do_shutdown(self, restart):
        if self._repl is not None:
            self._repl.terminate()
            self._repl = None
        return super().do_shutdown(restart)

    def _send_stream(self, name: str, text: str, silent: bool) -> None:
        if not silent and text:
            self.send_response(self.iopub_socket, "stream", {"name": name, "text": text})

    def _error(self, ename: str, evalue: str, silent: bool) -> dict:
        if not silent:
            self.send_response(
                self.iopub_socket,
                "error",
                {"ename": ename, "evalue": evalue, "traceback": []},
            )
        return {
            "status": "error",
            "execution_count": self.execution_count,
            "ename": ename,
            "evalue": evalue,
            "traceback": [],
        }


def _is_failure(event: Event) -> bool:
    match event:
        case Exited():
            return True
        case Stream(name="stderr", text=text):
            return bool(_ERROR_RE.search(_ANSI_RE.sub("", text)))
    return False


def _merge_streams(events: list[Event]) -> list[Event]:
    """Join adjacent output of the same stream into one message."""
    merged: list[Event] = []
    for event in events:
        last = merged[-1] if merged else None
        if isinstance(event, Stream) and isinstance(last, Stream) and last.name == event.name:
            merged[-1] = Stream(event.name, last.text + event.text)
        else:
            merged.append(event)
    return merged
