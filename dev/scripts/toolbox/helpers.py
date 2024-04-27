import click
import subprocess as sp


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


def bash_command(cmd, cwd=".", redirect=None):
    click.echo(click.style(f"[RUNNING BASH]: {cmd}", fg="yellow", bold=False))
    proc = sp.Popen(["/bin/bash", "-c", cmd], cwd=cwd, stdout=redirect, stderr=redirect)
    stdout, stderr = proc.communicate()
    return proc.wait(), stdout, stderr


def bash_command_get_output(cmd, cwd="."):
    return bash_command(cmd, cwd, redirect=sp.PIPE)


def log_info(msg):
    click.echo(click.style(f"[INFO]: {msg}", fg="yellow", bold=True))


def log_new_line():
    click.echo("")


def abort_if_false(ctx, param, value):
    if not value:
        ctx.abort()
