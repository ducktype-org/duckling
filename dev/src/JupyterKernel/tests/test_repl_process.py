# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""Drives a real `duckc repl --framed`; needs DUCKLING_KERNEL_COMMAND."""

import os
import shlex
import unittest

from duckling_kernel.protocol import Display, Exited, Result, Stream
from duckling_kernel.repl_process import ReplProcess

COMMAND = os.environ.get("DUCKLING_KERNEL_COMMAND")

DISPLAY_CELL = r"""import core.builtins.*;
fun emitDisplay() = {
    builtin_output_char(30u8 as char);
    builtin_output_str("@duck-display begin text/html\n<b>hi</b>\n");
    builtin_output_char(30u8 as char);
    builtin_output_str("@duck-display end\n");
}
emitDisplay();"""


@unittest.skipUnless(COMMAND, "DUCKLING_KERNEL_COMMAND is not set")
class ReplProcessTest(unittest.TestCase):
    def setUp(self):
        self.repl = ReplProcess(shlex.split(COMMAND))
        startup = list(self.repl.start())
        self.assertFalse([e for e in startup if isinstance(e, Exited)], startup)

    def tearDown(self):
        self.repl.terminate()

    def run_cell(self, code):
        return list(self.repl.run(code))

    def test_multiline_definition_then_result(self):
        self.assertEqual(self.run_cell("fun f(x: i64) -> i64 = {\n    return x + 1;\n}"), [])
        self.assertEqual(self.run_cell("f(41)"), [Result("42")])

    def test_program_output_is_stdout(self):
        events = self.run_cell("import core.builtins.*;\nbuiltin_output_i64(7);")
        self.assertIn(Stream("stdout", "7\n"), events)

    def test_errors_go_to_stderr(self):
        events = self.run_cell("missing_symbol")
        stderr = "".join(e.text for e in events if isinstance(e, Stream) and e.name == "stderr")
        self.assertIn("missing_symbol", stderr)
        self.assertEqual(self.run_cell("1 + 1"), [Result("2")])

    def test_display_block(self):
        events = self.run_cell(DISPLAY_CELL)
        self.assertEqual(events, [Display({"text/html": "<b>hi</b>"})])

    def test_exit_command(self):
        self.assertIsInstance(self.run_cell("/exit")[-1], Exited)


if __name__ == "__main__":
    unittest.main()
