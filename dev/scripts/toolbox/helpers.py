import subprocess as sp

import click


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


class BashCommandError(Exception):
    def __init__(self, command, exit_code, stdout, stderr):
        super().__init__(
            f"Bash command `{command}` has failed with a an exit code: {exit_code}, because: \n{stdout} ;;\n {stderr}"
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
        stdout = stdout.decode('UTF-8')
    if stderr is not None:
        stderr = stderr.decode('UTF-8')

    status = proc.wait()
    if status != 0:
        raise BashCommandError(cmd, status, stdout, stderr)
    return stdout, stderr


def bash_command_get_output(cmd, cwd="."):
    return bash_command(cmd, cwd, redirect=sp.PIPE)


def log_info(msg, newline=True):
    click.echo(click.style(f"[INFO]: {msg}", fg="yellow", bold=False), nl=newline)

def log_warning(msg, fg="magenta", newline=True):
    click.echo(click.style(f"[WARNING]: {msg}", fg=fg, bold=True), nl=newline)

def get_input(msg, newline=False):
    click.echo(click.style(f"[INPUT]: {msg}", fg="blue", bold=False), nl=newline)
    return input()

def log_new_line():
    click.echo("")


def abort_if_false(ctx, param, value):
    if not value:
        ctx.abort()
