from click import option, BOOL
from .helpers import (
    bash_command,
)

def impl(build_dir, memcheck):
    if memcheck:
        bash_command(f"cmake --build {build_dir} -- memcheck_test")
    else:
        bash_command(f"cmake --build {build_dir} -- test")
        
def build_dir(func):
    return option(
		"-b",
		"--build_dir",
		prompt="build directory with docs enabled",
		help="The name of the build directory with enabled docs.",
		default="build",
	)(func)

def memcheck(func):
    return option(
		"-m",
		"--memcheck",
		prompt="Memcheck",
		help="Whether or not to perform memcheck with valgrind",
		type=BOOL,
		default=False,
		show_default=True,
	)(func)