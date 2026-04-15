import os

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
        help="Path to clang-format, ex. /usr/bin/clang-format-20 or clang-format",
        default="clang-format-20",
    )(*args, **kwargs)


def clang_tidy(*args, **kwargs):
    return create_option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        type=str,
        help="Path to clang-tidy, ex. /usr/bin/clang-tidy-20 or clang-tidy",
        default="clang-tidy-20",
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


def get_cpu_count() -> int:
    # cpu_count might return None
    return os.cpu_count() or 1


def llvm_version(*args, **kwargs):
    return create_option(
        "-v",
        "--llvm-version",
        "llvm_version",
        prompt="LLVM Version",
        type=str,
        default="20.1.7",
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
