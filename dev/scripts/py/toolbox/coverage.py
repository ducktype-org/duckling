from pathlib import Path
from click import option

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
)

def impl(build_dir, thread_count):
    log_info("Running coverage...")
    if not Path(build_dir).exists():
        exit_with_error(f"Given build folder does not exist: {build_dir}.")

    thread_option = ""

    if thread_count == "default":
        pass
    elif thread_count.isdigit():
        thread_option = f"-j {int(thread_count)}"
    else:
        exit_with_error(
            f'Incorrect thread parameter: `{thread_count}`. Legal values are: numbers and "default".'
        )

    bash_command(f"cmake --build {build_dir} {thread_option} -- build_all_tests")
    bash_command(f"cmake --build {build_dir} {thread_option} -- test")

    bash_command(f"cmake --build {build_dir} -- coverage")

    bash_command("xdg-open coverage/index.html", cwd=build_dir)
    
def build_dir(func):
    return option(
		"-b",
		"--build_dir",
		prompt="Build directory",
		help="The name of the directory.",
		default="build",
	)(func)

def thread_count(func):
    return option(
		"-j",
		"--thread-count",
		prompt="Number of threads used when building",
		help="Number of threads used when building",
		type=str,
		default="default",
	)(func)