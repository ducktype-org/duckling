# @TODO, determine if it makes sense to use any of these decorators in mentioned file
from click import option
from ..impl.helpers import (
	default_compiler_from_ctx,
    BUILD_SYSTEMS,
)
from os import cpu_count

# issue_checker, duck_linter, cpp_linter
def no_merge_base(func):
    return option(
        "--no-merge-base",
        is_flag=True,
        default=False,
        help="On no-merge-base: compare against the latest commit on `branch` "
        "instead of the commit which is the LCA of `branch` and current branch. "
        "This feature allows to run the checker on a shallow clone.",
    )(func)

# duck_linter, cpp_linter
def all_flag(func):
    return option(
        "-a",
        "--all",
        is_flag=True,
        default=False,
        help="Check all files, not just the ones that are modified",
    )(func)

# cpp_linter, pr_validate
def format(func):
    return option(
        "-f",
        "--format",
        "clang_format_path",
        prompt="clang-format path",
        help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
        default="clang-format-19",
    )(func)

# cpp_linter, pr_validate
def tidy(func):
    return option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
        default="clang-tidy-19",
    )(func)

# downlad_llvm, install_llvm
def version(func):
    return option(
        "-v",
        "--version",
        prompt="LLVM Version",
        help="Version of LLVM release to compile, ex. 19.1.7",
        default="19.1.7",
    )(func)

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

# issue_checker, duck_linter, cpp_linter
def branch(func):
    return option(
        "-r",
        "--branch",
        help="The branch relative to which the diff is created.",
        type=str,
        default="origin/main",
    )(func)

# coverage, setup_build, itest?
def build_dir1(func):
    return option(
        "-b",
        "--build_dir",
        prompt="Build dir name",
        help="The name of the directory.",
        default="build",
    )(func)

# itest?, test, cpp_preprocessor, docs
def build_dir2(func):
    option(
        "-b",
        "--build_dir",
        prompt="build directory with docs enabled",
        help="The name of the build directory with enabled docs.",
        default="build",
    )(func)

# cpp_linter, coverage
def threads(func):
    return option(
        "-j",
        "--threads",
        help="On how many threads can linter use. Defaults to os.cpu_count()",
        type=int,
        default=cpu_count() or 1,
    )(func)

# install_llvm, setup_build
def build_system(func):
    return option(
        "-b",
        "--build-tool",
        "build_tool",
        prompt="Build system",
        help="Build system to use",
        default="Ninja",
        type=BUILD_SYSTEMS,
    )(func)

# duck_linter, itest
def verbose(func):
    return option(
        "-v",
        "--verbose",
        is_flag=True,
        default=False,
        help="Also shows checks files that didn't had any errors.",
    )(func)