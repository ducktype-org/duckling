import click
import subprocess


def exit_with_error(msg):
    click.echo(click.style("ERROR: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


def bash_command(cmd):
    click.echo(click.style(f"Running: {cmd}", fg="yellow"))
    proc = subprocess.Popen(["/bin/bash", "-c", cmd])
    proc.wait()


def log_info(msg):
    click.echo(msg)
