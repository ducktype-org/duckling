#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import sys
import re
from typing import Callable
from rich.console import Console
from rich.theme import Theme
from pathlib import Path

# color reference: https://rich.readthedocs.io/en/stable/appendix/colors.html
# style reference: https://rich.readthedocs.io/en/stable/style.html#defining-styles
theme_path = Path(__file__).parent.joinpath("ld_theme")
theme = Theme.read(theme_path.as_posix(), inherit=False)
console = Console(theme=theme)

# @TODO: python 3.11 allows for positive restrictions on things after the match

# The whole symbol part of the error
symbol = re.compile(r"(?<=`)(.*)(?![^'])")

# Things that look like paths excluding common things that are definetly not paths
path = re.compile(
    r"(?<![a-zA-Z0-9/._-])((?:[.]{2})?[a-zA-Z0-9/_-]+(?:[/]|[.])[a-zA-Z0-9/._-]*[a-zA-Z0-9_-])(?![a-zA-Z0-9/._-])"
)

# Numbers after paths, usually row numbers
row = re.compile(r"(?<=:)([0-9]+)(?![^:])")

# Errors
error = re.compile(r"([Ee]rror)")

# Descriptions of the problem
problem = re.compile(r"(?<=: )([a-zA-Z ]*)(?![^`])")

# Namespace identyfiers
namespace = re.compile(r"([a-zA-Z_-]*::)")

# Name identifiers
name = re.compile(r"([a-zA-Z_-]+)(?![a-zA-Z_-].|::)")
# Parenthesis that exist in symbols, should not add `[]` without very specific changes because of syntax

parenthesis = re.compile(r"([()<>])")

# References and pointers
refs = re.compile(r"([&*])")


def changeGroup(type_name: str, add: tuple[str, str]) -> Callable[[re.Match[str]], str]:
    """
    Returns a function that changes the first group to type `type_name`
    and adds add[0] and add[1] before and after the first group
    """

    def change(match: re.Match[str]) -> str:
        res = add[0] + "[" + type_name + "]"
        res += match.group(1)
        res += "[/" + type_name + "]" + add[1]
        for i in range(2, len(match.groups()) + 1):
            res += match.group(i)
        return res

    return change


def makeType(
    line: str, type_name: str, expr: re.Pattern[str], add: tuple[str, str] = ("", "")
) -> str:
    """
    Changes not-overlapping matches to `expr` from `line` to type `type_name`
    and adds add[0] and add[1] before and after the first group
    """
    return expr.sub(changeGroup(type_name, add), line)


def recursiveMakeType(
    line: str,
    type_name: str,
    exprs: list[re.Pattern[str]],
    add: tuple[str, str] = ("", ""),
) -> str:
    """
    Does the same as `makeType` but perform sequential matching of exprs.
    First mathes expr[0], then expr[1] only inside mathed regions, then expr[2], ... .
    """

    def inner(match: re.Match[str]) -> str:
        return recursiveMakeType(match.group(1), type_name, exprs[1:], add)

    if len(exprs) == 1:
        return makeType(line, type_name, exprs[0], add)
    else:
        return exprs[0].sub(inner, line)


def color_line(line: str) -> str:
    # Paths should always be replaced first,
    # unless we add a check for no `]` after (because of error syntax)
    line = makeType(line, "path", path)
    line = makeType(line, "error", error)
    line = makeType(line, "row", row)
    line = makeType(line, "problem", problem)

    # Names should always be replaced first among the ones in symbol,
    # unless we add a check for no `]` after (because of syntax)
    line = recursiveMakeType(line, "name", [symbol, name])
    line = recursiveMakeType(line, "namespace", [symbol, namespace])
    line = recursiveMakeType(line, "parenthesis", [symbol, parenthesis])
    line = recursiveMakeType(line, "refs", [symbol, refs])

    return line


def main():
    for line in sys.stdin:
        # Adding newlines between errors in different functions
        if re.search(r"in function", line):
            print("")
        out = color_line(line)
        console.print(out, end="")


if __name__ == "__main__":
    main()
