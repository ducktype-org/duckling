# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from pathlib import Path

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
)


def coverage_impl(build_dir, thread_count):
    # Preamble
    log_info("Running coverage...")
    build_path = Path(build_dir)
    if not build_path.exists():
        exit_with_error(f"Given build directory does not exist: {build_dir}.")

    # Rebuild, retest, recompute coverage
    bash_command(f"cmake --build {build_dir} -j {thread_count} -- build_all_tests")
    bash_command(f"cmake --build {build_dir} -j {thread_count} -- test")

    bash_command(f"cmake --build {build_dir} -- coverage")

    # Open coverage report
    bash_command("xdg-open coverage/index.html", cwd=build_dir)
