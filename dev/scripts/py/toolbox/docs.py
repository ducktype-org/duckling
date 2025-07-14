from click import option, command
from pathlib import Path
from .helpers import (
    bash_command,
    exit_with_error,
)


def docs_impl(build_dir):
    bash_command(f"cmake --build {build_dir} -- docs")
    bash_command(f"cmake --build {build_dir} -- open-sphinx-docs")

@command()
@option(
	"-b",
	"--build_dir",
	prompt="build directory with docs enabled",
	help="The name of the build directory with enabled docs.",
	default="build",
)
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_impl(*args, **kwargs)
    
if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    docs()