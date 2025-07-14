from click import option, BOOL, command
from .helpers import (
    bash_command,
    exit_with_error,
)
from pathlib import Path

def test_impl(build_dir, memcheck):
    if memcheck:
        bash_command(f"cmake --build {build_dir} -- memcheck_test")
    else:
        bash_command(f"cmake --build {build_dir} -- test")


@command()
@option(
    "-b",
    "--build_dir",
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
    default="build",
)
@option(
    "-m",
    "--memcheck",
    prompt="Memcheck",
    help="Whether or not to perform memcheck with valgrind",
    type=BOOL,
    default=False,
    show_default=True,
)
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_impl(*args, **kwargs)

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    test()
