from click import option, Choice
from ..impl.helpers import (
	default_compiler_from_ctx,
)

BUILD_SYSTEMS = Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

def no_merge_base(func):
    return option(
        "--no-merge-base",
        is_flag=True,
        default=False,
        help="On no-merge-base: compare against the latest commit on `branch` "
        "instead of the commit which is the LCA of `branch` and current branch. "
        "This feature allows to run the checker on a shallow clone.",
    )(func)

def all_flag(func):
    return option(
        "-a",
        "--all",
        is_flag=True,
        default=False,
        help="Check all files, not just the ones that are modified",
    )(func)

def format(func):
    return option(
        "-f",
        "--format",
        "clang_format_path",
        prompt="clang-format path",
        help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
        default="clang-format-19",
    )(func)

def tidy(func):
    return option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
        default="clang-tidy-19",
    )(func)

def version(func):
    return option(
        "-v",
        "--version",
        prompt="LLVM Version",
        help="Version of LLVM release to compile, ex. 19.1.7",
        default="19.1.7",
    )(func)

# @TODO determine how to deal with options about the compilers
# install_llvm, setup_build
def c_compiler(func):
    return option(
        "-c",
        "--cc-compiler",
        prompt="C compiler path",
        help="A path to the C complier to compile with",
        cls=default_compiler_from_ctx("cc_compiler"),
    )(func)

# install_llvm, setup_build
def cxx_compiler(func):
    option(
        "-x",
        "--cxx-compiler",
        prompt="C++ compiler path",
        help="A path to the C++ complier to compile with",
        # this overrides the click.Option class to use the default_compiler_from_ctx
        # instead, so it can get ctx and infer and set the default value
        cls=default_compiler_from_ctx("cxx_compiler"),
    )(func)

def branch(func):
    return option(
        "-r",
        "--branch",
        help="The branch relative to which the diff is created.",
        type=str,
        default="origin/main",
    )(func)

def build_dir(prompt, help):
    return option(
        "-b",
        "--build_dir",
        prompt=prompt,
        help=help,
        default="build",
    )

def thread_count(help):
    return option(
        "-j",
        "--thread-count",
        prompt="Number of threads to use",
        help=help,
        type=str,
        default="default",
    )

def build_system(func):
    return option(
        "-b",
        "--build_system",
        "build_system",
        prompt="Build system",
        help="Build system to use",
        default="Ninja",
        type=BUILD_SYSTEMS,
    )(func)

def verbose(help):
    return option(
        "-v",
        "--verbose",
        is_flag=True,
        default=False,
        help=help,
    )