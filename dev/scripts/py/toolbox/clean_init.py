from click import option, command
from pathlib import Path
from .helpers import (
    bash_command,
    log_info,
    abort_if_false,
    exit_with_error,
)

def clean_init_impl():
    log_info("Removing .venv and downloaded binaries...")
    bash_command("rm -rf .venv")
    bash_command(
        "find . ! -name '.gitignore' -type f -exec rm -r {} +", cwd="scripts/downloads/"
    )

@command()
@option(
    "--yes",
    is_flag=True,
    callback=abort_if_false,
    expose_value=False,
    prompt="This operation deletes files, are you sure?",
)
def clean_init():
    """Removes things created by init."""
    clean_init_impl()

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    clean_init()