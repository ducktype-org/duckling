from click import option

from .helpers import (
    bash_command,
    log_info,
    abort_if_false
)

def impl():
    log_info("Removing .venv and downloaded binaries...")
    bash_command("rm -rf .venv")
    bash_command(
        "find . ! -name '.gitignore' -type f -exec rm -r {} +", cwd="scripts/downloads/"
    )
    
def yes_flag(func):
    return option(
		"--yes",
		is_flag=True,
		callback=abort_if_false,
		expose_value=False,
		prompt="This operation deletes files, are you sure?",
	)(func)