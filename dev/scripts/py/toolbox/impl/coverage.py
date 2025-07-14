from pathlib import Path

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
)

def coverage_impl(build_dir, thread_count):
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
