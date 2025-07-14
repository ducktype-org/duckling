from .helpers import (
    bash_command,
    log_info,
    log_new_line,
    exit_with_error
)
from pathlib import Path
from .download_binaries import download_binaries_impl
from .setup_venv import setup_venv_impl
from click import command

def init_impl():
    log_info("Initializing the REPO!...")
    log_new_line()

    log_info("Initializing git submodules...")
    bash_command("git submodule update --init")
    log_new_line()

    setup_venv_impl()
    log_new_line()

    download_binaries_impl(False)
    log_new_line()

@command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc..."""
    init_impl()

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    init()