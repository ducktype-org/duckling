#!/usr/bin/python3

import click
import os

from scripts.toolbox.helpers import bash_command, exit_with_error, log_info
from scripts.toolbox.internet_file import (
    InternetFile,
    callback_chmod,
    callback_move,
    callback_remove,
    callback_unTARXZ,
)


DATA_USER = "internal"
DATA_PASS = "1Aasjviedhvo="
BUILD_SYSTEMS = click.Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)


FILES_TO_DOWNLOAD: list[InternetFile] = [
    InternetFile(
        "scripts/downloads/clang-format",
        "https://static.ducktype.org/bin/clang-format",
        auth=(DATA_USER, DATA_PASS),
        after_download=[(callback_chmod, "clang-format", "u+x")],
    ),
    InternetFile(
        "scripts/downloads/ccache.tar.xz",
        "https://github.com/ccache/ccache/releases/download/v4.9.1/ccache-4.9.1-linux-x86_64.tar.xz",
        after_download=[
            (callback_unTARXZ,),
            (callback_move, "ccache-4.9.1-linux-x86_64/ccache", "ccache"),
            (callback_remove, "ccache-4.9.1-linux-x86_64"),
        ],
    ),
]


@click.group()
def cli():
    pass


def with_venv(cmd):
    if not os.path.exists(".venv"):
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    bash_command(f"source .venv/bin/activate && {cmd}")


def setup_build_impl(name, build_system, type, docs, compiler, ccache):
    """Makes a build folder"""

    cmd = f"""
        cmake
         -G "{build_system}"
         -B {name}
         -D CMAKE_BUILD_TYPE={type}
         -D BUILD_DOCS={'ON' if docs else 'OFF'}
         -D CMAKE_CXX_COMPILER={compiler}
         -D USE_CCACHE={'ON' if ccache else 'OFF'}
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
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_impl(*args, **kwargs)


def setup_venv_impl():
    if not os.path.exists(".venv"):
        log_info("Creating venv...")
        bash_command("python3 -m venv .venv")
        log_info("Downloading venv dependencies...")
        with_venv("python3 -m pip install -r docs/doc-config/requirements.txt")


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


@cli.command()
@click.option(
    "-f",
    "--force",
    help="Whether or not to force the download",
    is_flag=True,
    type=bool,
    default=False,
)
def download_binaries(force):
    """Download binary files from the internet"""
    download_binaries_impl(force)


@cli.command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc.."""
    log_info("Initializing git submodules...")
    bash_command("git submodule update --init")
    setup_venv_impl()
    download_binaries_impl(False)


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
def coverage(name, build_system):
    if not os.path.exists(name):
        exit_with_error(f"Given build folder does not exist: {name}.")

    with_venv(f"cmake -D ENABLE_COVERAGE=true -B {name}")
    os.chdir(name)
    bash_command(f"cmake --build . -j 5 -- test")

    build_system = build_system.lower()
    if "ninja" in build_system:
        bash_command("ninja coverage")
    if "makefile" in build_system:
        bash_command("make coverage")

    bash_command("xdg-open coverage/index.html")


if __name__ == "__main__":
    if not "dev/./toolbox.py" in __file__:
        raise RuntimeError("Toolbox should be called from the root of the project")

    cli()
