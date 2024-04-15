import click
import subprocess


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


def bash_command(cmd):
    click.echo(click.style(f"[RUNNING BASH]: {cmd}", fg="yellow", bold=True))
    proc = subprocess.Popen(["/bin/bash", "-c", cmd])
    proc.wait()


def log_info(msg):
    click.echo(click.style(f"[INFO]: {msg}", fg="yellow", bold=True))

def log_new_line():
    click.echo("")