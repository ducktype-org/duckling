from .helpers import (
    bash_command,
    log_info,
)


def clean_init_impl():
    log_info("Removing .venv and downloaded binaries...")
    bash_command("rm -rf .venv")
    bash_command(
        "find . ! -name '.gitignore' -type f -exec rm -r {} +", cwd="scripts/downloads/"
    )
