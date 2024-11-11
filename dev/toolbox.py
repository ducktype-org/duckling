#!/usr/bin/python3

import pathlib
import sys
import shutil

import click
import requests

from scripts.toolbox.helpers import (
    abort_if_false,
    bash_command,
    exit_with_error,
    log_info,
    log_new_line,
)
from scripts.toolbox.internet_file import (
    InternetFile,
    callback_chmod,
    callback_move,
    callback_remove,
    callback_unTAR,
)

from scripts.toolbox.cpp_linter import simulate_cpp_linter

DATA_USER = "dev"
# @FUTURE: change this password and hide it:
DATA_PASS = "7ocwXWOAwg="
BUILD_SYSTEMS = click.Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

FILES_TO_DOWNLOAD: list[InternetFile] = [
    InternetFile(
        "scripts/downloads/clang-format",
        "http://internal.ducktype.org/static/bin/clang-format",
        auth=(DATA_USER, DATA_PASS),
        after_download=[(callback_chmod, "clang-format", "u+x")],
    ),
    InternetFile(
        "scripts/downloads/ccache.tar.xz",
        "https://github.com/ccache/ccache/releases/download/v4.9.1/ccache-4.9.1-linux-x86_64.tar.xz",
        after_download=[
            (callback_unTAR,),
            (callback_move, "ccache-4.9.1-linux-x86_64/ccache", "ccache"),
            (callback_remove, "ccache-4.9.1-linux-x86_64"),
        ],
    ),
]


@click.group()
def cli():
    pass


def with_venv(cmd):
    if not pathlib.Path(".venv").exists():
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    bash_command(f"source .venv/bin/activate && {cmd}")


def setup_build_impl(build_dir, build_system, type, docs, compiler, ccache, coverage):
    bld = pathlib.Path(build_dir)
    if bld.exists():
        # Delete old cache
        try:
            (bld / pathlib.Path("CMakeCache.txt")).unlink()
            shutil.rmtree(bld / pathlib.Path("CMakeFiles"))
        except FileNotFoundError:
            pass
    cmd = f"""
        cmake
         -G "{build_system}"
         -B {build_dir}
         -D CMAKE_BUILD_TYPE={type}
         -D BUILD_DOCS={'ON' if docs else 'OFF'}
         -D CMAKE_CXX_COMPILER={compiler}
         -D USE_CCACHE={'ON' if ccache else 'OFF'}
         -D ENABLE_COVERAGE={'true' if coverage else 'false'}
    """
    cmd = cmd.replace("\n", " ")

    log_info("Setting up a build folder...")
    if docs:
        with_venv(cmd)
    else:
        bash_command(cmd)


@cli.command()
@click.option(
    "-b",
    "--build_dir",
    prompt="Build dir name",
    help="The name of the directory.",
    default="build",
)
@click.option(
    "-s",
    "--build-system",
    prompt="Build system",
    help="The build system to use",
    type=BUILD_SYSTEMS,
    default="Ninja",
)
@click.option(
    "-t",
    "--type",
    prompt="build type",
    help="The build type.",
    default="debug",
    type=click.Choice(
        ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], case_sensitive=False
    ),
)
@click.option(
    "-d",
    "--docs",
    prompt="Build docs",
    help="Whether or not to build the docs.",
    type=bool,
    default=True,
    is_flag=True,
)
@click.option(
    "-c",
    "--compiler",
    prompt="Compiler path",
    help="A path to the complier to compile with",
    default="g++",
)
@click.option(
    "--ccache",
    prompt="Use ccache",
    help="Whether or not to use ccache.",
    type=bool,
    default=False,
    is_flag=True,
)
@click.option(
    "--coverage",
    prompt="Enable coverage",
    help="Whether or not to enable coverage",
    type=bool,
    default=False,
    is_flag=True,
)
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_impl(*args, **kwargs)


def setup_venv_impl():
    if not pathlib.Path(".venv").exists():
        log_info("Creating venv...")
        bash_command("python3 -m venv .venv")
        log_info("Downloading venv dependencies...")
        with_venv("python3 -m pip install -r docs/doc-config/requirements.txt")
        log_info("Done creating venv.")
    else:
        log_info("Venv already exits. Skip.")


@cli.command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()


def download_binaries_impl(force=False, single=False):
    log_info(
        f"Downloading binary files {'WITH force' if force else 'WITHOUT force'}..."
    )

    if single:
        log_info(f"Searching for file called '{single}'...")

        found = False
        for file in FILES_TO_DOWNLOAD:
            if single in file.resource_url:
                log_info(f"Found file: {file.resource_url}")
                file.download(force)
                found = True
                break
        if not found:
            exit_with_error(f"Couldn't find a file with '{single}' in resource url")

    else:
        log_info(f"Downloading all supported files")
        for file in FILES_TO_DOWNLOAD:
            file.download(force)

    log_info("Download done")


@cli.command()
@click.option(
    "-f",
    "--force",
    help="Whether or not to force the download of files that already exits",
    is_flag=True,
    type=bool,
    default=False,
)
@click.option(
    "-s",
    "--single",
    help="Download a single file, that is fuzzily named as passed in this flag",
    type=str,
    default="",
)
def download_binaries(*args, **kwargs):
    """Downloads necessary binary files from the internet"""
    download_binaries_impl(*args, **kwargs)


def init_impl():
    log_info("Initializing the REPO!...")
    log_new_line()

    log_info("Initializing git submodules...")
    bash_command("git submodule update --init")
    log_new_line()

    setup_venv_impl()
    log_new_line()

    download_binaries_impl(False)
    log_new_line()


@cli.command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc..."""
    init_impl()


@cli.command()
@click.option(
    "-b",
    "--build_dir",
    prompt="Build directory",
    help="The name of the directory.",
    default="build",
)
@click.option(
    "-j",
    "--thread-count",
    prompt="Number of threads used when building",
    help="Number of threads used when building",
    type=str,
    default="default",
)
def coverage(build_dir, thread_count):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""

    log_info("Running coverage...")
    if not pathlib.Path(build_dir).exists():
        exit_with_error(f"Given build folder does not exist: {build_dir}.")

    thread_option = ""

    if thread_count == "default":
        pass
    elif thread_count.isdigit():
        thread_option = f"-j {int(thread_count)}"
    else:
        exit_with_error(
            f'Incorrect thread parameter: `{thread_count}`. Legal values are: numbers and "default".'
        )

    bash_command(f"cmake --build {build_dir} {thread_option} -- build_all_tests")
    bash_command(f"cmake --build {build_dir} {thread_option} -- test")

    bash_command(f"cmake --build {build_dir} -- coverage")

    bash_command("xdg-open coverage/index.html", cwd=build_dir)


@cli.command()
@click.option(
    "--yes",
    is_flag=True,
    callback=abort_if_false,
    expose_value=False,
    prompt="This operation deletes files, are you sure?",
)
def clean_init():
    """Removes things created by init."""
    log_info("Removing .venv and downloaded binaries...")
    bash_command("rm -rf .venv")
    bash_command(
        "find . ! -name '.gitignore' -type f -exec rm -r {} +", cwd="scripts/downloads/"
    )


def docs_impl(build_dir):
    bash_command(f"cmake --build {build_dir} -- docs")
    bash_command(f"cmake --build {build_dir} -- open-sphinx-docs")


@cli.command()
@click.option(
    "-b",
    "--build_dir",
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
    default="build",
)
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_impl(*args, **kwargs)


def test_impl(build_dir, memcheck):
    if memcheck:
        bash_command(f"cmake --build {build_dir} -- memcheck_test")
    else:
        bash_command(f"cmake --build {build_dir} -- test")


@cli.command()
@click.option(
    "-b",
    "--build_dir",
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
    default="build",
)
@click.option(
    "-m",
    "--memcheck",
    prompt="Memcheck",
    help="Whether or not to perform memcheck with valgrind",
    type=bool,
    default=False,
    is_flag=True,
)
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_impl(*args, **kwargs)


def download_llvm_impl(version, arch):
    log_info("==========================")
    log_info(
        "Downloading LLVM may or may not work, depending on a presence of compiled binaries listed here: https://github.com/llvm/llvm-project/releases/"
    )
    log_new_line()
    log_info("- A note on binaries -")
    log_info("Volunteers make binaries for the LLVM project, which will be uploaded")
    log_info("when they have had time to test and build these binaries. They might")
    log_info("not be available directly or not at all for each release. We suggest")
    log_info("you use the binaries from your distribution or build your own if you")
    log_info("rely on a specific platform or configuration.")
    log_info("==========================")
    log_new_line()

    if arch == "x86_64":
        name = f"clang+llvm-{version}-{arch}-linux-gnu-ubuntu-18.04"
    else:
        name = f"clang+llvm-{version}-{arch}-linux-gnu"

    llvm_file = InternetFile(
        f"scripts/downloads/llvm_{version}_{arch}.tar.xz",
        f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/{name}.tar.xz",
        after_download=[
            (callback_unTAR,),
            (callback_move, name, f"llvm_lib_{version}_{arch}"),
        ],
    )
    llvm_file.download()


@cli.command()
@click.option(
    "-v",
    "--version",
    prompt="LLVM Version",
    help="Version of LLVM release, ex. 18.1.8",
    default="18.1.8",
)
@click.option(
    "-a",
    "--arch",
    prompt="Architecture",
    help="Architecture of the target machine",
    default="x86_64",
    type=click.Choice(["x86_64", "aarch64"], case_sensitive=False),
)
def download_llvm(*args, **kwargs):
    """Downloads specified version of LLVM. This is LINUX ONLY."""
    download_llvm_impl(*args, **kwargs)


@cli.command()
@click.option(
    "-t",
    "--tidy",
    "clang_tidy_path",
    prompt="clang-tidy path",
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-17 or clang-tidy",
    default="clang-tidy-17",
)
@click.option(
    "-f",
    "--format",
    "clang_format_path",
    prompt="clang-format path",
    help="Path to clang-format, ex. /usr/bin/clang-format-17 or clang-format",
    default="scripts/downloads/clang-format",
)
@click.option(
    "-b",
    "--build",
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
    default="build",
)
@click.option(
    "-j",
    "--threads",
    help="On how many threads can linter run?",
    type=int,
    default=4,
)
@click.option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
def linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    simulate_cpp_linter(*args, **kwargs)


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    # Disable traceback for shorter error messages.
    # Comment this line when debugging.
    sys.tracebacklimit = 0

    cli()
