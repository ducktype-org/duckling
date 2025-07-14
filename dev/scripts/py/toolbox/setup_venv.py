from pathlib import Path
from os import path as os_path
from sys import executable
from click import command

from .helpers import (
    bash_command,
    log_info,
    with_venv,
    exit_with_error,
)

def setup_venv_impl():
    if not Path(".venv").exists():
        log_info("Creating venv...")
        bash_command(executable + " -m venv .venv")
        log_info("Downloading venv dependencies...")
        venv_python = os_path.join(".venv", "bin", "python")
        with_venv(venv_python + " -m pip install -r docs/doc-config/requirements.txt")
        log_info("Done creating venv.")
    else:
        log_info("Venv already exits. Skip.")

@command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    setup_venv()
