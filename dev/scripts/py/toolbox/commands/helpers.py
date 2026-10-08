# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import os
from pathlib import Path

from click import Option, UsageError, option, Choice


def create_option(*def_arg, **def_kwargs):
    """
    Creates a customizable `click.option` decorator with predefined defaults,
    allowing overrides at the point of use.
    """

    def specialize_option(*arg, **kwargs):
        return option(*(arg or def_arg), **{**def_kwargs, **kwargs})

    return specialize_option


# HERE DEFINE REPEATING FLAGS


def all_flag(*args, **kwargs):
    return create_option(
        "-a",
        "--all",
        "all",
        is_flag=True,
        type=bool,
        default=False,
    )(*args, **kwargs)


def branch(*args, **kwargs):
    return create_option(
        "-r",
        "--branch",
        "branch",
        type=str,
        default="origin/main",
    )(*args, **kwargs)


def build_dir(*args, **kwargs):
    return create_option(
        "-b",
        "--build-dir",
        "build_dir",
        prompt="Build directory",
        type=str,
        default="build",
    )(*args, **kwargs)


def build_system(*args, **kwargs):
    return create_option(
        "-s",
        "--build-system",
        "build_system",
        prompt="Build system",
        default="Ninja",
        type=Choice(["Ninja", "Unix Makefiles"], case_sensitive=False),
    )(*args, **kwargs)


def clang_format(*args, **kwargs):
    return create_option(
        "-f",
        "--format",
        "clang_format_path",
        prompt="clang-format path",
        type=str,
        help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
        default="clang-format-19",
    )(*args, **kwargs)


def clang_tidy(*args, **kwargs):
    return create_option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        type=str,
        help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
        default="clang-tidy-19",
    )(*args, **kwargs)


def cxx_compiler(*args, **kwargs):
    return create_option(
        "-x",
        "--cxx-compiler",
        "cxx_compiler",
        prompt="C++ compiler path",
    )(*args, **kwargs)


def cc_compiler(*args, **kwargs):
    return create_option(
        "-c",
        "--cc-compiler",
        "cc_compiler",
        prompt="C compiler path",
    )(*args, **kwargs)


def _cgroup_cpu_limit() -> int:
    """Threads allowed by this process's CPU cgroup, or 0 when it is unrestricted.

    Mirrors the `Set number of threads` step in .github/workflows/tests.yml, including
    its `+ 1`.
    """
    # cgroup v2: "<quota> <period>", or "max <period>" when there is no quota.
    try:
        quota, period = Path("/sys/fs/cgroup/cpu.max").read_text().split()
        if quota != "max" and int(period) > 0:
            return int(quota) // int(period) + 1
        return 0
    except (OSError, ValueError):
        pass
    # cgroup v1, where an unrestricted quota is -1.
    try:
        quota = int(Path("/sys/fs/cgroup/cpu/cpu.cfs_quota_us").read_text())
        period = int(Path("/sys/fs/cgroup/cpu/cpu.cfs_period_us").read_text())
        if quota > 0 and period > 0:
            return quota // period + 1
    except (OSError, ValueError):
        pass
    return 0


def get_cpu_count() -> int:
    """Threads this process may actually use.
    """
    limit = _cgroup_cpu_limit()
    try:
        available = len(os.sched_getaffinity(0))  # Linux only
    except AttributeError:  # macOS, Windows
        available = os.cpu_count() or 1
    return min(limit, available) if limit else available


def llvm_version(*args, **kwargs):
    return create_option(
        "-v",
        "--llvm-version",
        "llvm_version",
        prompt="LLVM Version",
        type=str,
        default="19.1.7",
    )(*args, **kwargs)


def no_merge_base(*args, **kwargs):
    return create_option(
        "--no-merge-base",
        is_flag=True,
        type=bool,
        help="On no-merge-base: compare against the latest commit on `branch`"
        "instead of the commit which is the LCA of `branch` and current branch."
        "This feature allows using a shallow clone.",
        default=False,
    )(*args, **kwargs)


def thread_count(*args, **kwargs):
    return create_option(
        "-j",
        "--thread-count",
        "thread_count",
        help="Number of threads used. Defaults to the number of available threads.",
        default=get_cpu_count(),
        type=int,
    )(*args, **kwargs)


def verbose(*args, **kwargs):
    return create_option(
        "-v",
        "--verbose",
        "verbose",
        is_flag=True,
        default=False,
    )(*args, **kwargs)


def auto_fix(*args, **kwargs):
    return create_option(
        "--auto-fix",
        is_flag=True,
        help="Apply fixes automatically instead of prompting.",
        default=False,
        cls=MutuallyExclusiveOption,
        mutually_exclusive=["no_fix"],
    )(*args, **kwargs)


def no_fix(*args, **kwargs):
    return create_option(
        "--no-fix",
        is_flag=True,
        help="Do not apply automatic fixes, only report them.",
        default=False,
        cls=MutuallyExclusiveOption,
        mutually_exclusive=["auto_fix"],
    )(*args, **kwargs)


## HERE DEFINE HELPER CLASSES USED IN OPTIONS


class MutuallyExclusiveOption(Option):
    # Thanks to: https://stackoverflow.com/questions/37310718/mutually-exclusive-option-groups-in-python-click
    def __init__(self, *args, **kwargs):
        self.mutually_exclusive = set(kwargs.pop("mutually_exclusive", []))
        help = kwargs.get("help", "")
        if self.mutually_exclusive:
            ex_str = ", ".join(self.mutually_exclusive)
            kwargs["help"] = help + (
                " NOTE: This argument is mutually exclusive with "
                " arguments: [" + ex_str + "]."
            )
        super(MutuallyExclusiveOption, self).__init__(*args, **kwargs)

    def handle_parse_result(self, ctx, opts, args):
        if self.mutually_exclusive.intersection(opts) and self.name in opts:
            raise UsageError(
                "Illegal usage: `{}` is mutually exclusive with "
                "arguments `{}`.".format(self.name, ", ".join(self.mutually_exclusive))
            )

        return super(MutuallyExclusiveOption, self).handle_parse_result(ctx, opts, args)
