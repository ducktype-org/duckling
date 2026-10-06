# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.cloc import cloc_impl
from click import argument, command, option, Path


@command()
@argument(
    "paths",
    nargs=-1,
    type=Path(exists=True),
)
@option(
    "-f",
    "--by-file",
    is_flag=True,
    default=False,
    help="Report the counts of every single file instead of a per-language summary.",
)
@option(
    "--all-files",
    is_flag=True,
    default=False,
    help="Count untracked files as well, instead of only the ones tracked by git.",
)
@option(
    "--cloc-path",
    type=str,
    default="cloc",
    help="Path to the cloc executable, ex. /usr/bin/cloc or cloc.",
)
def cloc(*args, **kwargs):
    """
    Counts the lines of code in the repository using `cloc`.

    Duckling's languages are unknown to `cloc`, so the counting is done with the
    `duckling.cloc` language definition living at the root of the repository.

    Counts the whole repository when no PATHS are given, ex.
    `./toolbox.py cloc src/vm`.
    """
    cloc_impl(*args, **kwargs)
