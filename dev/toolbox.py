#!/usr/bin/python3

import pathlib
import click

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


def setup_build_impl(name, build_system, type, docs, compiler, ccache, coverage):
    """Makes a build folder"""

    cmd = f"""
        cmake
         -G "{build_system}"
         -B {name}
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
    "-n",
    "--name",
    prompt="name",
    help="The name of the directory.",
    default="build",
)
@click.option(
    "-b",
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


def download_binaries_impl(force=False):
    log_info(
        f"Downloading binary files {'WITH force' if force else 'WITHOUT force'}..."
    )
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
def download_binaries(force):
    """Download necessary binary files from the internet"""
    download_binaries_impl(force)


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
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc.."""
    init_impl()


@cli.command()
@click.option(
    "-n",
    "--name",
    prompt="name",
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
def coverage(name, thread_count):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""

    log_info("Running coverage...")
    if not pathlib.Path(name).exists():
        exit_with_error(f"Given build folder does not exist: {name}.")

    thread_option = ""

    if thread_count == "default":
        pass
    elif thread_count.isdigit():
        thread_option = f"-j {int(thread_count)}"
    else:
        exit_with_error(
            f'Incorrect thread parameter: `{thread_count}`. Legal values are: numbers and "default".'
        )

    bash_command(f"cmake --build {name} {thread_option} -- build_all_tests")
    bash_command(f"cmake --build {name} {thread_option} -- test")

    bash_command(f"cmake --build {name} -- coverage")

    bash_command("xdg-open coverage/index.html", cwd=name)


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


def docs_impl(name):
    bash_command(f"cmake --build {name} -- docs")
    bash_command("xdg-open docs/sphinx/index.html", cwd=name)


@cli.command()
@click.option(
    "-n",
    "--name",
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
    default="build",
)
def docs(*args, **kwargs):
    docs_impl(*args, **kwargs)


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    cli()
