#!/usr/bin/python3

import click
import os
import enum


def exit_with_error(msg):
    click.echo(msg, fg="red")
    exit(1)


class ShellType(enum.Enum):
    BASH = 1
    FISH = 2
    ZSH = 3


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


def active_venv_impl():
    if not os.path.exists(".venv"):
        exit_with_error(".venv does not exits. Use ./toolbox.py setup_venv")
    sh = get_shell()
    if sh == ShellType.FISH:
        os.system("source .venv/bin/activate.fish")
    else:
        os.system("source .venv/bin/activate")


@cli.command()
def active_venv():
    """Activates a python's virtual environment"""
    active_venv_impl()


def build_impl(name, build_system, type, docs, compiler):
    """Makes a build folder"""
    if docs:
        active_venv_impl()
    os.system(
        f"cmake -G {build_system} -B {name}-{type.lower()} -D CMAKE_BUILD_TYPE={type}"
        f" -D BUILD_DOCS={'ON' if docs else 'OFF'} -D CMAKE_CXX_COMPILER={compiler}"
    )


@cli.command()
@click.option(
    "-n",
    "--name",
    prompt="name",
    help="The name of the directory.",
    type=str,
    default="build",
)
@click.option(
    "-b",
    "--build-system",
    prompt="Build system",
    help="The build system to use",
    type=click.Choice(["Ninja", "Unix Makefiles"], case_sensitive=False),
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
def build(*args, **kwargs):
    """Makes a build folder"""
    build_impl(*args, **kwargs)


def setup_venv_impl():
    if not os.path.exists(".venv"):
        os.system("python3 -m venv .venv")
        active_venv_impl()
        os.system("python3 -m pip install -r docs/doc-config/requirements.txt")


@cli.command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()


@cli.command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc.."""
    os.system("git submodule update --init")
    active_venv_impl()


if __name__ == "__main__":
    cli()
