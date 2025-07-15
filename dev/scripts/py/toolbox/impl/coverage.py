from pathlib import Path

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
    get_cpu_count,
)


def coverage_impl(build_dir, thread_count):
    # Preamble
    log_info("Running coverage...")
    build_path = Path(build_dir)
    if not build_path.exists():
        exit_with_error(f"Given build directory does not exist: {build_dir}.")

    thread_option = ""

    if thread_count == "default":
        thread_option = f"-j {get_cpu_count()}"
    elif thread_count.isdigit():
        thread_option = f"-j {int(thread_count)}"
    else:
        exit_with_error(
            f'Incorrect thread parameter: `{thread_count}`. Legal values are: numbers and "default".'
        )

    # Rebuild, retest, recompute coverage
    bash_command(f"cmake --build {build_dir} {thread_option} -- build_all_tests")
    bash_command(f"cmake --build {build_dir} {thread_option} -- test")

    bash_command(f"cmake --build {build_dir} -- coverage")

    # Open coverage report
    bash_command("xdg-open coverage/index.html", cwd=build_dir)
