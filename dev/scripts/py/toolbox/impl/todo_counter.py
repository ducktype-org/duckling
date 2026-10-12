# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from .helpers import (
    bash_command_get_output,
    log_info,
)


def todo_counter_impl(branch: str, count_only: bool, pattern: list[str]):

    if len(pattern) == 0:
        patterns = ["TODO", "FIXME"]
    else:
        patterns = pattern

    def show_counts(branch, patterns):
        assert len(patterns) >= 1
        patterns = map(lambda x: f' -e "{x}" ', patterns)
        patterns_grep_str = "".join(patterns)
        cmd = f"git grep -I -o -i {patterns_grep_str} {branch} | wc -l"
        out, _ = bash_command_get_output(cmd)
        return int(out)

    if not count_only:
        log_info(f'Branch {branch if branch != "" else "Current branch"}:')
    for pattern in patterns:
        count = show_counts(branch, [pattern])
        if not count_only:
            log_info(f"{pattern}: {count}")
        else:
            print(count)
