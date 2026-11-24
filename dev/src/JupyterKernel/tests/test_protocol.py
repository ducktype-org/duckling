# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import unittest

from duckling_kernel.protocol import (
    DISPLAY_BEGIN,
    DISPLAY_END,
    END_OF_INPUT,
    READY,
    RESULT,
    Display,
    Ready,
    Result,
    StdoutParser,
    Stream,
    frame_input,
)


def parse(*lines: str):
    parser = StdoutParser()
    events = []
    for line in lines:
        events += parser.feed_line(line)
    return events + parser.flush()


class ProtocolTest(unittest.TestCase):
    def test_frame_input_ends_with_marker_line(self):
        self.assertEqual(frame_input("a\nb\n"), f"a\nb\n{END_OF_INPUT}\n")

    def test_plain_lines_are_stdout(self):
        self.assertEqual(parse("hello", ""), [Stream("stdout", "hello\n"), Stream("stdout", "\n")])

    def test_result(self):
        self.assertEqual(parse(RESULT + "42"), [Result("42")])

    def test_ready_after_unterminated_output(self):
        self.assertEqual(parse("End_of_completions" + READY), [Stream("stdout", "End_of_completions\n"), Ready()])

    def test_display_parts_form_one_bundle(self):
        events = parse(
            DISPLAY_BEGIN + "text/html",
            "<b>",
            "x</b>",
            DISPLAY_END,
            DISPLAY_BEGIN + "text/plain",
            "x",
            DISPLAY_END,
            "after",
        )
        self.assertEqual(
            events,
            [Display({"text/html": "<b>\nx</b>", "text/plain": "x"}), Stream("stdout", "after\n")],
        )

    def test_bundle_flushed_before_ready(self):
        events = parse(DISPLAY_BEGIN + "image/svg+xml", "<svg/>", DISPLAY_END, READY)
        self.assertEqual(events, [Display({"image/svg+xml": "<svg/>"}), Ready()])

    def test_marker_lines_inside_a_part_are_payload(self):
        events = parse(DISPLAY_BEGIN + "text/plain", RESULT + "1", DISPLAY_END)
        self.assertEqual(events, [Display({"text/plain": RESULT + "1"})])

    def test_unterminated_part_is_kept_as_text(self):
        events = parse(DISPLAY_BEGIN + "text/html", "<p>")
        self.assertEqual(events, [Stream("stdout", DISPLAY_BEGIN + "text/html\n<p>\n")])


if __name__ == "__main__":
    unittest.main()
