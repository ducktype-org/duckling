from concurrent.futures import ThreadPoolExecutor
from functools import lru_cache
from pathlib import Path
import os
import platform
import subprocess
import sys
import tempfile

from .helpers import (
    BashCommandError,
    bash_command,
    bash_command_get_output,
    exit_with_error,
    get_input,
    log_good,
    log_info,
    log_new_line,
    log_warning,
)
from .list_files import list_files_impl


def cpp_linter_impl(
    clang_tidy_path: str | None,
    clang_format_path: str | None,
    build_dir: str,
    thread_count: int,
    branch: str = "origin/main",
    all: bool = False,
    no_merge_base: bool = False,
    auto_fix: bool = False,
    no_fix: bool = False,
) -> tuple[bool, bool]:
    """
    Perform C++ linting using clang-tidy and clang-format.

    Linter can run only clang_tidy, only clang-format, or both.
    If you want to run only one of them, set the other path to None.
    If both paths are None, the linter will exit with an error.

    Args:
        clang_tidy_path: Path to the clang-tidy executable
        clang_format_path: Path to the clang-format executable
        build_dir: Path to the build directory
        thread_count: Number of threads to use for parallel linting
        branch: Git branch to compare against for modified files
        all: If True, lint all C++ files in the repo; otherwise, only modified files
        no_merge_base: If True, skip merge base calculation when determining modified files
        auto_fix: If True, apply automatic fixes when possible
        no_fix: If True, do not apply automatic fixes, only report them
    """

    if not clang_tidy_path and not clang_format_path:
        exit_with_error(
            "At least one of clang_tidy_path or clang_format_path must be set"
        )

    build_folder = Path(build_dir)
    if not build_folder.exists():
        exit_with_error(f"Given build folder does not exist: {build_folder.absolute()}")

    file_diffs = get_files_for_linter(all, branch, no_merge_base)
    log_info(f"Found {file_diffs=}")

    clang_format_failed = False
    clang_tidy_failed = False

    with ThreadPoolExecutor(max_workers=thread_count) as e:

        def call_linter(fd: tuple[str, list[tuple[int, int]]]):
            return run_linter_on(clang_tidy_path, clang_format_path, build_folder, *fd)

        results = e.map(call_linter, file_diffs.items())

        for logs, ct_failed, cf_failed in results:
            sys.stdout.write(logs)
            clang_tidy_failed |= ct_failed
            clang_format_failed |= cf_failed

    if clang_format_failed and not no_fix:
        log_new_line()
        apply = auto_fix
        if not auto_fix:
            to_format = get_input("Found formatting issues. Format the repo [Y/n]: ")
            apply = to_format.lower() in ["y", "yes", ""]
        if apply:
            bash_command(f"./scripts/formatting/format_repo_cpp.sh {clang_format_path}")
            clang_format_failed = False

    if clang_tidy_path and not clang_tidy_failed:
        log_good("clang-tidy found no issues in the checked files")
    if clang_format_path and not clang_format_failed:
        log_good("clang-format: all checked files are properly formatted")

    return clang_tidy_failed, clang_format_failed


def get_repo_cpp_files() -> dict[str, list[tuple[int, int]]]:
    # Use the new standardized file listing for C++ files
    return list_files_impl(
        extensions=[".cpp", ".hpp", ".cc", ".cxx", ".h"],
        only_modified=False,
        lines=True,
    )


def get_modified_files_and_lines(
    branch: str, no_merge_base: bool = False
) -> dict[str, list[tuple[int, int]]]:
    # Use the shared implementation from list_files module
    return list_files_impl(
        branch=branch, no_merge_base=no_merge_base, only_modified=True, lines=True
    )


def get_files_for_linter(
    all: bool, branch: str, no_merge_base: bool
) -> dict[str, list[list[int]]]:
    if all:
        files_to_lint = get_repo_cpp_files()
    else:
        files_to_lint = get_modified_files_and_lines(branch, no_merge_base)

    # Transform the list of line ranges from a list of tuples to a list of lists.
    # So it prints out nicely and fits the JSON like format accepted by clang.
    return {
        filename: [list(line_range) for line_range in line_ranges]
        for filename, line_ranges in files_to_lint.items()
    }


@lru_cache(maxsize=None)
def gcc_clang_tidy_extra_args(build_folder: Path) -> str:
    """
    clang-tidy parses translation units with clang. On macOS a build configured with GCC
    (g++/libstdc++) leaves clang unable to find libstdc++ or the macOS SDK headers on its own,
    so every file fails with e.g. `'type_traits' file not found`. Return the --extra-arg flags
    that point clang at the GCC libstdc++ headers and the SDK. Empty on non-macOS or non-GCC
    builds (Apple/Homebrew clang builds use libc++ and need no help).
    """
    if platform.system() != "Darwin":
        return ""

    compiler = ""
    try:
        for line in (build_folder / "CMakeCache.txt").read_text().splitlines():
            if line.startswith("CMAKE_CXX_COMPILER:"):
                compiler = line.split("=", 1)[1].strip()
                break
    except OSError:
        return ""

    if not compiler:
        return ""
    name = Path(compiler).name.lower()
    # Rule out clang first: "clang++" contains "g++" as a substring.
    if "clang" in name:
        return ""
    if "g++" not in name and "gcc" not in name:
        return ""

    # Ask the GCC driver for its libstdc++ header search paths.
    include_dirs: list[str] = []
    try:
        search = subprocess.run(
            [compiler, "-std=c++23", "-E", "-x", "c++", "-v", "/dev/null"],
            capture_output=True,
            text=True,
        ).stderr
    except OSError:
        return ""
    in_search = False
    for line in search.splitlines():
        if "#include <...> search starts here:" in line:
            in_search = True
        elif "End of search list." in line:
            break
        elif in_search and "/c++/" in line:
            include_dirs.append(os.path.realpath(line.strip()))

    if not include_dirs:
        return ""

    # -nostdinc++ drops clang's own C++ stdlib search (which would resolve to libc++ and warn that
    # libstdc++ wasn't found); the -isystem paths below supply libstdc++ instead.
    args = ["--extra-arg=-nostdinc++"]
    try:
        sdk = subprocess.run(
            ["xcrun", "--show-sdk-path"], capture_output=True, text=True
        ).stdout.strip()
        if sdk:
            args += ["--extra-arg=-isysroot", f"--extra-arg={sdk}"]
    except OSError:
        pass
    for include_dir in include_dirs:
        args += ["--extra-arg=-isystem", f"--extra-arg={include_dir}"]
    return " ".join(args)


def clang_tidy_on(
    clang_tidy_path: str,
    build_folder: Path,
    file: str,
    file_diffs: list[tuple[int, int]],
    log_file,
) -> bool:
    """
    Runs clang-tidy on a file with given file_diffs
    Returns False if no errors were found, True otherwise.
    """

    # clang-tidy command succeeds if no errors were found.
    # Prints warnings on stdout.
    try:
        tidy_out, _ = bash_command_get_output(
            f"{clang_tidy_path} -p {build_folder} --format-style file --config-file .clang-tidy"
            f' --line-filter="[{{"name": "{file}", "lines": {file_diffs}}}]"'
            f" --extra-arg=-std=c++23 {gcc_clang_tidy_extra_args(build_folder)} {file}",
            log_file=log_file,
        )
        if tidy_out:
            log_warning(f"clang-tidy output: \n{tidy_out}", file=log_file)
            return True
    except BashCommandError as e:
        log_warning(
            f"clang-tidy failed: {file}, because:\n{e.stdout}{e.stderr}", file=log_file
        )
        return True
    return False


def clang_format_on(
    clang_format_path: str, file: str, file_diffs: list[tuple[int, int]], log_file
) -> bool:
    """
    Dry-run clang-format on a file with given file_diffs to test
    if it's properly formatted. Returns False if so, True otherwise.
    """

    # clang-format command succeeds always and returns data (possibly empty)
    lines = [f"--lines={start}:{stop}" for start, stop in file_diffs]
    format_out, format_err = bash_command_get_output(
        f"{clang_format_path}" " -style=file --dry-run" f" {' '.join(lines)} {file}",
        log_file=log_file,
    )

    # Print data returned by clang-format
    if format_out or format_err:
        log_warning(f"clang-format output: \n{format_out}{format_err}", file=log_file)
        return True
    return False


def run_linter_on(
    clang_tidy_path: str | None,
    clang_format_path: str | None,
    build_folder: Path,
    file,
    diff,
) -> tuple[str, bool, bool]:
    clang_format_failed = False
    clang_tidy_failed = False
    logs = ""
    if file.endswith(".hpp") or file.endswith(".cpp"):
        log_file = tempfile.TemporaryFile("w+")
        log_info(f"Linting: {file}", file=log_file)

        if clang_tidy_path and clang_tidy_on(
            clang_tidy_path, build_folder, file, diff, log_file
        ):
            clang_tidy_failed = True

        if clang_format_path and clang_format_on(
            clang_format_path, file, diff, log_file
        ):
            clang_format_failed = True

        log_file.seek(0)
        logs = log_file.read()

    else:
        log_info(f"Skipping linting on: {file}")

    return logs, clang_tidy_failed, clang_format_failed
