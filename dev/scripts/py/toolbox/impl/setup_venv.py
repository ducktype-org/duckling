from pathlib import Path
from os import path as os_path
from sys import executable

from .helpers import (
    bash_command,
    log_info,
    with_venv,
)

"""
Path to requirements.txt file relative to "dev/" directory 
(where toolbox.py is).
"""
requirements_path = "requirements.txt"


def setup_venv_impl():
    if not Path(".venv").exists():
        log_info("Creating venv...")
        bash_command(executable + " -m venv .venv")
        log_info("Downloading venv dependencies...")
        venv_python = os_path.join(".venv", "bin", "python")
        with_venv(venv_python + " -m pip install -r " + requirements_path)
        log_info("Done creating venv.")
    else:
        log_info("Venv already exits. Skip.")
