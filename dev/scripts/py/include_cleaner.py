#!/usr/bin/env python3

"""
Parser for the output of include-what-you-use, which *only removes* stuff.
This way we don't pedantically include <vector> in every other file.

This file processes the output of `iwyu_tool`, which you can run run like so:
```
$ iwyu_tool -p $BUILD_DIRECTORY -j0 > $OUTPUT_FILE
$ ./scripts/py/include-cleaner.py $OUTPUT_FILE
```

The scripts expects to be run from the `dev` directory.

IWYU catches a lot of unused files, more than the clangd editor hints,
but this naive remove-only approach also produces a lot of false positives.
Removing them can be very laborious, so this script is meant to be run
every couple of months when someone has time to sift through its work,
it's not meant to be a CI step.
"""


from typing import Iterator, Optional
import sys
import re

REMOVE_SECTION_RE = re.compile(r"^(.+) should remove these lines:$")
REMOVE_LINE_RE = re.compile(r"^- (.+?)\s+// lines (\d+)-(\d+)$")

LineMatches = dict[int, str]  # from lineno to content


def skipUntilNextRemoveHeader(lines: Iterator[str]) -> Optional[str]:
    while (line := next(lines, None)) is not None:
        if match := REMOVE_SECTION_RE.match(line):
            filename = match.group(1)
            return filename
    return None


def collectLineMatches(lines: Iterator[str]) -> Optional[LineMatches]:
    result = {}
    while (line := next(lines, None)) is not None:
        if match := REMOVE_LINE_RE.match(line):
            content, fromLine, toLine = match.groups()
            fromLine, toLine = int(fromLine), int(toLine)

            # Never witnessed those during script development
            assert fromLine == toLine, "Multiline removals not supported"

            result[fromLine] = content
        else:
            break
    if len(result) == 0:
        # Sometimes empty remove sections are generated, let's skip them
        return None
    else:
        return result


def parseFile(lines: Iterator[str]) -> dict[str, LineMatches]:
    result = {}
    while (filename := skipUntilNextRemoveHeader(lines)) is not None:
        if (matches := collectLineMatches(lines)) is not None:
            result[filename] = (result.get(filename) or {}) | matches
    return result


def clean(filename: str, matches: LineMatches) -> None:
    with open(filename, "r") as f:
        lines = f.readlines()

    new_lines = []
    for number, l in enumerate(lines, start=1):
        if number in matches:
            cleaned_line = l.split("//")[0].strip()
            iwyu_entry = matches[number]

            if cleaned_line not in iwyu_entry:
                print(
                    f"Unexpected line content, perhaps you ran the program twice on the same data\n"
                    f"  in file {filename}:{number}\n"
                    f"  Expected: {iwyu_entry}\n"
                    f"  Found: {l}\n",
                    file=sys.stderr,
                )
                exit(1)
        else:
            new_lines.append(l)

    with open(filename, "w") as f:
        f.write("".join(new_lines))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"USAGE: {sys.argv[0]} <IWYU_OUTPUT>")
        exit(1)

    iwyu_out_filename = sys.argv[1]

    with open(iwyu_out_filename, "r") as f:
        lines = iter(f.readlines())

    for filename, matches in parseFile(lines).items():
        clean(filename, matches)
