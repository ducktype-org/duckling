from .helpers import bash_command_get_output, log_info
from click import option

def impl(branch: str, count_only: bool, pattern: list[str]):

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

def branch(func):
    return option(
        "-b",
        "--branch",
        type=str,
        default="",
        help="Branch for which to count (empty string means the current branch). It can also be any other commit reference understood be git (e.g. HEAD~1)",
    )(func)

def count_only_flag(func):
    return option(
        "-c",
        "--count-only",
        is_flag=True,
        default=False,
        help="Only show the counts and nothing else",
    )(func)

def pattern(func):
    return option(
        "-p",
        "--pattern",
        type=str,
        required=False,
        multiple=True,
        help="Search for a given pattern instead of the default ones (todo and fixme). If passed multiple times, all the patterns will be searched for",
    )(func)