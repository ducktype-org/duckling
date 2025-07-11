from click import option

from .helpers import (
    bash_command,
)

def impl(build_dir):
    bash_command(f"cmake --build {build_dir} -- docs")
    bash_command(f"cmake --build {build_dir} -- open-sphinx-docs")
    
def build_dir(func):
    return option(
		"-b",
		"--build_dir",
		prompt="build directory with docs enabled",
		help="The name of the build directory with enabled docs.",
		default="build",
	)(func)