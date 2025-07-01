#!/usr/bin/env python3

import os
import pathlib
import sys
import shutil

import click

from scripts.py.toolbox.integration.tester import (
    DEFAULT_LOG_FILE_PATH,
    integration_tests_impl,
)
from scripts.py.toolbox.helpers import (
    abort_if_false,
    bash_command,
    exit_with_error,
    get_llvm_strings,
    get_llvm_source_strings,
    log_info,
    log_new_line,
    with_venv,
    default_compiler_from_ctx,
    check_if_compilers_are_compatible,
    log_warning,
)
from scripts.py.toolbox.internet_file import (
    InternetFile,
    callback_move,
    callback_remove,
    callback_unTAR,
)
from scripts.py.toolbox.pr_validate import pr_validate_impl
from scripts.py.toolbox.issue_checker import issue_checker_impl

from scripts.py.toolbox.cpp_linter import simulate_cpp_linter
from scripts.py.toolbox.duck_linter import duck_linter_impl
from scripts.py.toolbox.todo_counter import todo_counter_impl

# @todo move all other implementation to separate files

DATA_USER = "dev"
# @FUTURE: change this password and hide it:
DATA_PASS = "7ocwXWOAwg="
BUILD_SYSTEMS = click.Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

FILES_TO_DOWNLOAD: list[InternetFile] = [
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


def setup_build_impl(
    build_dir,
    build_system,
    type,
    docs,
    cxx_compiler,
    cc_compiler,
    gcov_version,
    ccache,
    coverage,
):

    check_if_compilers_are_compatible(cxx_compiler, cc_compiler)

    bld = pathlib.Path(build_dir)
    if bld.exists():
        # Delete old cache
        try:
            (bld / pathlib.Path("CMakeCache.txt")).unlink()
            shutil.rmtree(bld / pathlib.Path("CMakeFiles"))
        except FileNotFoundError:
            pass
    cmd = " ".join(
        [
            f"cmake",
            f'-G "{build_system}"',
            f"-B {build_dir}",
            f"-D CMAKE_BUILD_TYPE={type}",
            f"-D BUILD_DOCS={'ON' if docs else 'OFF'}",
            f"-D CMAKE_CXX_COMPILER={cxx_compiler}",
            f"-D CMAKE_C_COMPILER={cc_compiler}",
            f"-D GCOV_VERSION={gcov_version}",
            f"-D USE_CCACHE={'ON' if ccache else 'OFF'}",
            f"-D ENABLE_COVERAGE={'true' if coverage else 'false'}",
        ]
    )

    log_info("Setting up a build folder...")
    if docs or coverage:
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
    "-x",
    "--cxx-compiler",
    prompt="C++ compiler path",
    help="A path to the C++ complier to compile with",
    # this overrides the click.Option class to use the default_compiler_from_ctx
    # instead, so it can get ctx and infer and set the default value
    cls=default_compiler_from_ctx("cxx_compiler"),
)
@click.option(
    "-c",
    "--cc-compiler",
    prompt="C compiler path",
    help="A path to the C complier to compile with",
    # this overrides the click.Option class to use the default_compiler_from_ctx
    # instead, so it can get ctx and infer and set the default value
    cls=default_compiler_from_ctx("cc_compiler"),
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
# @TODO: make it prompt only for cov-build (see https://click.palletsprojects.com/en/stable/options/#callbacks-and-eager-options)
@click.option(
    "--gcov-version",
    prompt="GCOV version",
    help="GCOV version that will be passed to find_program in CMAKE",
    default="gcov-14",
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
    type=click.BOOL,
    default=False,
    show_default=True,
)
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_impl(*args, **kwargs)


def download_llvm_impl(version, os, arch):
    log_info("==========================")
    log_warning(
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

    link, downloaded, extracted, friendly = get_llvm_strings(version, os, arch)

    llvm_file = InternetFile(
        downloaded,
        link,
        after_download=[
            (callback_unTAR,),
        ],
    )
    llvm_file.download()


@cli.command()
@click.option(
    "-c",
    "--confirm",
    prompt=(
        "From LLVM 19 onwards, the releases are compiled with unfavourable compile options, so it is recommended to either:\n"
        " - use the LLVM from your distribution (e.g. apt install llvm-19)\n"
        " - build LLVM from source (see `install-llvm` command)\n"
        "Do you want to continue with the download?"
    ),
    type=bool,
    default=True,
    is_flag=True,
)
@click.option(
    "-v",
    "--version",
    prompt="LLVM Version",
    help="Version of LLVM release, ex. 19.1.4",
    default="19.1.7",
)
@click.option(
    "-o",
    "--os",
    prompt="Operating system",
    help="Operating system of the target machine",
    default="linux",
    type=click.Choice(["Linux", "macOS", "Windows"], case_sensitive=False),
)
@click.option(
    "-a",
    "--arch",
    prompt="Architecture",
    help="Architecture of the target machine",
    default="X64",
    type=click.Choice(["X64", "ARM64"], case_sensitive=False),
)
def download_llvm(*args, **kwargs):
    """Downloads the specified version of LLVM."""
    download_llvm_impl(*args, **kwargs)


def install_llvm_impl(version, ram_gb, c_compiler, cxx_compiler, linker, build_tool, targets, source_dir_path, use_old_build):
    log_info("==========================")
    log_info("Building LLVM from source. This will take significant time!")
    log_info(
        "A single target build with lld and Ninja takes about 8 minutes on a 10-thread machine"
    )
    log_info(
        "You can also use LLVM from your distribution (e.g. apt install llvm-19) instead."
    )
    log_info("==========================")
    log_new_line()

    source_dir_path = pathlib.Path(os.path.expanduser(source_dir_path))

    if not source_dir_path.exists():
        log_info(f"Creating LLVM source directory at {source_dir_path}")
        source_dir_path.mkdir(parents=True, exist_ok=True)

    link, downloaded_name, extracted_name, friendly_name = get_llvm_source_strings(
        version
    )
    extracted_name = pathlib.Path(extracted_name)
    if not (source_dir_path / extracted_name).exists():
        # 1. Download to downloads directory
        download_dir = pathlib.Path("scripts/downloads")
        downloaded_path = download_dir / pathlib.Path(downloaded_name)

        llvm_file = InternetFile(
            str(downloaded_path),
            link,
            after_download=[
                (callback_unTAR,),
            ],
        )
        llvm_file.download()

        extracted_dir = download_dir / extracted_name
        bash_command(f"mv {extracted_dir} {source_dir_path}")
    else:
        log_info(
            f"LLVM source directory already exists at {source_dir_path / extracted_name}"
        )

    sources_path = source_dir_path / extracted_name
    install_dir = (pathlib.Path("scripts/downloads") / friendly_name).absolute()

    # Calculate parallel link jobs based on RAM
    link_jobs = max(1, int(ram_gb) // 16)
    log_info(f"Using {link_jobs} parallel link jobs based on {ram_gb}GB RAM")

    # Create build directory
    build_dir = sources_path / "build"
    if not use_old_build:
        # remove old build directory if it exists
        if build_dir.exists():
            log_info(f"Removing old build directory: {build_dir}")
            shutil.rmtree(build_dir)

    if not build_dir.exists():
        build_dir.mkdir()

    # Configure LLVM build
    log_info("Configuring LLVM build...")

    # Build the cmake command
    cmake_cmd_parts = [
        f"cmake -S {sources_path}/llvm -B {build_dir}",
        f"-G '{build_tool}'",
        f"-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        f"-DLLVM_TARGETS_TO_BUILD={targets}",
        f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}",
    ]

    # Add linker option only if a specific linker is selected
    if linker != "default":
        cmake_cmd_parts.append(f"-DLLVM_USE_LINKER={linker}")

    if cxx_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_CXX_COMPILER={cxx_compiler}")
    if c_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_C_COMPILER={c_compiler}")

    cmake_command = " \\\n  ".join(cmake_cmd_parts)
    log_info(f"Running cmake command...")
    bash_command(cmake_command)

    # Build LLVM
    log_info("Building LLVM (this may take a while)...")
    bash_command(f"cmake --build {build_dir} -- -j{os.cpu_count() - 1}")

    # Install LLVM
    log_info("Installing LLVM...")
    bash_command(f"cmake --build {build_dir} --target install")

    log_info(f"LLVM {version} has been built and installed to {install_dir}")
    log_new_line()


@cli.command()
@click.option(
    "-v",
    "--version",
    prompt="LLVM Version",
    help="Version of LLVM release to compile, ex. 19.1.7",
    default="19.1.7",
)
@click.option(
    "-r",
    "--ram",
    "ram_gb",
    prompt="Available RAM (GB)",
    help="Amount of RAM available for linking (1 link job per 16GB)",
    default="16",
    type=str,
)
@click.option(
    "--cxx_compiler",
    prompt="C++ compiler path",
    help="A path to the C++ compiler to compile with",
    default="default",
    type=str
)
@click.option(
    "--c-compiler",
    prompt="C compiler path",
    help="A path to the C compiler to compile with",
    default="default",
    type=str
)
@click.option(
    "-l",
    "--linker",
    prompt="Linker to use (using one of the newer linkers (lld/mold) will speed up the compilation)",
    help="Linker to use for building LLVM.",
    default="lld",
    type=click.Choice(["default", "lld", "mold"], case_sensitive=False),
)
@click.option(
    "-b",
    "--build-tool",
    "build_tool",
    prompt="Build system",
    help="Build system to use",
    default="Ninja",
    type=BUILD_SYSTEMS,
)
@click.option(
    "-t",
    "--targets",
    prompt="LLVM targets to build (using 'all' will increase the build time about 3 times).",
    help="LLVM architecture targets to build.",
    default="Native",
    type=click.Choice(["Native", "X86", "all"], case_sensitive=False),
)
@click.option(
    "-s",
    "--source-dir",
    "source_dir_path",
    prompt="LLVM source directory",
    help="Directory where LLVM sources will be extracted",
    default="~/llvm",
    type=str,
)
@click.option(
    "--use-old-build",
    prompt="Use old build directory",
    help="Whether or not to use the old build directory",
    default=False,
    type=bool
)
def install_llvm(*args, **kwargs):
    """Compiles LLVM from source with specified options.

    This command will download LLVM source code, build it with the specified options,
    and install it to the 'scripts/downloads/installed' directory.
    Important! Is is advised to try to use the LLVM from your
    distribution (e.g. apt install llvm-19) first.
    """
    install_llvm_impl(*args, **kwargs)


@cli.command()
@click.option(
    "-t",
    "--tidy",
    "clang_tidy_path",
    prompt="clang-tidy path",
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
    default="clang-tidy-19",
)
@click.option(
    "-f",
    "--format",
    "clang_format_path",
    prompt="clang-format path",
    help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
    default="clang-format-19",
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
    help="On how many threads can linter use. Defaults to os.cpu_count()",
    type=int,
    default=os.cpu_count() or 1,
)
@click.option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@click.option(
    "-a",
    "--all",
    is_flag=True,
    default=False,
    help="Check all files, not just the ones that are modified",
)
@click.option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the linter on a shallow clone.",
)
def linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    clang_tidy_failed, clang_format_failed = simulate_cpp_linter(*args, **kwargs)
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"Linter has failed because: {clang_tidy_failed=}, {clang_format_failed=}"
        )


@cli.command()
@click.option(
    "-a",
    "--all",
    is_flag=True,
    default=False,
    help="Check all files, not just the ones that are modified",
)
@click.option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@click.option(
    "-v",
    "--verbose",
    is_flag=True,
    default=False,
    help="Also shows checks files that didn't had any errors.",
)
@click.option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the linter on a shallow clone.",
)
def duck_linter(*args, **kwargs):
    """Check for violations of
    some of the C++ coding guidelines for Duckling project.
    Current checks:
    * relative import check

    For details see dev-guides.
    """
    passed = duck_linter_impl(*args, **kwargs)
    if not passed:
        exit_with_error("Linting failed.")


@cli.command()
@click.option(
    "-c",
    "--clean",
    is_flag=True,
    default=False,
    help="Runs `Clean` command on every test. If passed, no tests are ran.",
)
@click.option(
    "-d",
    "--dry",
    is_flag=True,
    default=False,
    help="Only prints commands to be executed instead of really executing them",
)
@click.option(
    "-t",
    "--filter",
    type=str,
    default="",
    help="Run tests under the specified path prefix (e.g., 'tests/C++' or 'tests/C++/Case1').",
)
@click.option(
    "-f",
    "--fail-fast",
    is_flag=True,
    default=False,
    help="Whether to fail upon a testcase failure. If not passed, runs all tests regardless of their result.",
)
@click.option(
    "-v",
    "--verbose",
    is_flag=True,
    default=False,
    help="Prints some debug information about test cases",
)
@click.option(
    "-l",
    "--log-file",
    type=str,
    default=str(DEFAULT_LOG_FILE_PATH),
    help="Path to a log file. A log file contains e.g. dumps of program incorrect IO",
)
@click.option(
    "-b",
    "--build_dir",
    prompt="Build directory",
    help="The name of the project build directory which is passed to the framework.",
    default="build",
)
def itest(*args, **kwargs):
    """Runs integration tests"""
    integration_tests_impl(*args, **kwargs)


@cli.command()
@click.option(
    "-b",
    "--branch",
    type=str,
    default="",
    help="Branch for which to count (empty string means the current branch). It can also be any other commit reference understood be git (e.g. HEAD~1)",
)
@click.option(
    "-c",
    "--count-only",
    is_flag=True,
    default=False,
    help="Only show the counts and nothing else",
)
@click.option(
    "-p",
    "--pattern",
    type=str,
    required=False,
    multiple=True,
    help="Search for a given pattern instead of the default ones (todo and fixme). If passed multiple times, all the patterns will be searched for",
)
def todo_counter(*args, **kwargs):
    """Prints counts of todos and similar comments in the code"""
    todo_counter_impl(*args, **kwargs)


@cli.command()
@click.option(
    "-t",
    "--tidy",
    "clang_tidy_path",
    prompt="clang-tidy path",
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
    default="clang-tidy-19",
)
@click.option(
    "-f",
    "--format",
    "clang_format_path",
    prompt="clang-format path",
    help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
    default="clang-format-19",
)
@click.option(
    "-b",
    "--build",
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
    default="build",
)
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, linter, duck-linter, issue-checker.
    In the future we might add integration tests.
    """
    pr_validate_impl(*args, **kwargs)


@cli.command()
@click.argument("issues", nargs=-1, type=str)
@click.option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@click.option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the checker on a shallow clone.",
)
def issue_checker(issues, branch, no_merge_base):
    """Checks for occurrences of #issue_number in source files and prints file, line, and summary.

    If no issue numbers are provided, the script will attempt to fetch them from GitHub using the 'gh' CLI.
    You must be authenticated with 'gh' for this to work.
    """
    if not issue_checker_impl(issues, branch, no_merge_base):
        exit_with_error("Issue checker found issues numbers related to this pull request in the code")


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    # Disable traceback for shorter error messages.
    # Comment this line when debugging.
    sys.tracebacklimit = 0

    cli()
