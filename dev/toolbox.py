#!/usr/bin/python3

from dataclasses import dataclass
import click
import subprocess
import os
import enum
import urllib.request


class ShellType(enum.Enum):
    BASH = 1
    FISH = 2
    ZSH = 3


BUILD_SYSTEMS = click.Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

def bash_command(cmd):
    click.echo(click.style(f"Running: {cmd}", fg="yellow"))
    proc = subprocess.Popen(['/bin/bash', '-c', cmd])
    proc.wait()

@dataclass
class InternetFile:
    path: str
    resource_url: str

    def download(self, force=False):
        if not force and os.path.exists(os.path.join(self.path)):
            return
        urllib.request.urlretrieve(self.resource_url, self.path)


FILES_TO_DOWNLOAD: list[InternetFile] = [
    InternetFile(
        "scripts/formatting/clang-format",
        "https://static.ducktype.org/bin/clang-format",
    )
]


def exit_with_error(msg):
    click.echo(click.style("ERROR: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


def get_shell() -> ShellType:
    if "bash" in os.environ["SHELL"]:
        return ShellType.BASH
    if "fish" in os.environ["SHELL"]:
        return ShellType.FISH
    if "zsh" in os.environ["SHELL"]:
        return ShellType.ZSH
    return ShellType.BASH


@click.group()
def cli():
    pass


def with_venv(cmd):
    if not os.path.exists(".venv"):
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    if get_shell() == ShellType.FISH:
        bash_command(f"source .venv/bin/activate.fish && {cmd}")
    else:
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

    click.echo("Setting up a build folder...")
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
        click.echo("Creating venv...")
        bash_command("python3 -m venv .venv")
        click.echo("Downloading venv dependencies...")
        with_venv("python3 -m pip install -r docs/doc-config/requirements.txt")


@cli.command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()


def download_binaries_impl(force=False):
    click.echo("Downloading binary files...")
    for file in FILES_TO_DOWNLOAD:
        file.download(force)


@cli.command()
def download_binaries():
    """Download binary files from the internet"""
    download_binaries_impl(True)


@cli.command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc.."""
    click.echo("Initializing git submodules...")
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
    # TODO: Chdir to root.
    cli()
