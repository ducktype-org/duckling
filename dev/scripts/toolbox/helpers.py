import pathlib
import subprocess as sp
import sys

import click


def with_venv(cmd):
    if not pathlib.Path(".venv").exists():
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    bash_command(f"source .venv/bin/activate && {cmd}")


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


class BashCommandError(Exception):
    def __init__(self, command: str, exit_code, stdout, stderr):
        super().__init__(
            f"\n\tBash command `{command}` has failed with a an exit code: {exit_code}, because:\n"
            + f"[STDOUT]:{"\n" + stdout if stdout else ""}\n"
            + f"[STDERR]:{"\n" + stderr if stderr else ""}"
        )
        self.command = command
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr


def bash_command(cmd, cwd=".", redirect=None):
    click.echo(click.style(f"[BASH]: {cmd}", fg="bright_cyan", bold=False))
    proc = sp.Popen(["/bin/bash", "-c", cmd], cwd=cwd, stdout=redirect, stderr=redirect)
    stdout, stderr = proc.communicate()

    if stdout is not None:
        stdout = stdout.decode("UTF-8")
    if stderr is not None:
        stderr = stderr.decode("UTF-8")

    status = proc.wait()
    if status != 0:
        raise BashCommandError(cmd, status, stdout, stderr)
    return stdout, stderr


def bash_command_get_output(cmd, cwd="."):
    return bash_command(cmd, cwd, redirect=sp.PIPE)


def log_info(msg, newline=True, file=sys.stdout):
    click.echo(click.style(f"[INFO]: {msg}", fg="yellow", bold=False), nl=newline, file=file)


def log_warning(msg, newline=True, file=sys.stdout):
    click.echo(click.style(f"[WARNING]: {msg}", fg="magenta", bold=True), nl=newline, file=file)


def get_input(msg, newline=False):
    click.echo(click.style(f"[INPUT]: {msg}", fg="blue", bold=False, blink=True), nl=newline)
    return input()


def log_new_line(file=sys.stdout):
    click.echo("", file=file)


def abort_if_false(ctx, param, value):
    if not value:
        ctx.abort()


def make_pretty_command(command):
    pretty_command = command.replace("\n", " ")
    while "  " in pretty_command:
        pretty_command = pretty_command.replace("  ", " ")
    return pretty_command
