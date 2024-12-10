from concurrent.futures import ThreadPoolExecutor
import pathlib
import sys
import tempfile
from scripts.toolbox.helpers import (
    BashCommandError,
    bash_command,
    bash_command_get_output,
    exit_with_error,
    get_input,
    log_info,
    log_new_line,
    log_warning,
)


def get_unstaged_new_files() -> bool:
    status_out, _ = bash_command_get_output("git status --porcelain")
    new_unstaged_files = []
    for file in status_out.splitlines():
        if file.startswith("??"):
            new_unstaged_files.append(file[3:])

    return new_unstaged_files


def get_diffs(branch: str):
    if new_unstaged_files := get_unstaged_new_files():
        log_warning(
            f"Files not in working tree, so not included in diff: [{', '.join(new_unstaged_files)}]"
        )

    diff_out, _ = bash_command_get_output(
        f"git diff --merge-base {branch} -U0 --relative"
    )
    diff_lines = diff_out.splitlines()

    changes = {}

    prev_line = None
    filename = None
    for line in diff_lines:
        file_deleted = prev_line == "+++ /dev/null"

        if line.startswith("@@") and not file_deleted:
            # Check if this diff is for a new file...
            if prev_line.startswith("+++ b/"):
                # File has changed
                filename = prev_line[len("+++ b/") :].rstrip()

            # Parse diffed lines:
            # @@ -{line_start},{num_lines} +{line_start},{num_lines} @@ ...
            # or
            # @@ -{line_start} +{line_start} @@ ...
            diffed = line[line.find("+") + 1 :]
            diffed = diffed[: diffed.find("@@")]

            line_range = None
            if "," in diffed:
                # In this case there were multiple lines changed in format: +{line_start},{num_lines}
                line_start, num_lines = diffed.split(",")
                line_start = int(line_start)
                num_lines = int(num_lines)
                line_range = [line_start, line_start + num_lines]

                # This is for the format:
                # @@ -16 +15,0 @@
                # -#include <iostream>
                # (Which is very odd)
                if line_range[1] == 0:
                    continue
            else:
                # In this case there is only 1 line changed
                line_start = int(diffed)
                line_range = [line_start, line_start + 1]

            # Save to our dict which files have changed and which haven't.
            # There may be multiple places in one files with changed lines, so we have a list of ranges.
            if filename not in changes:
                changes[filename] = []

            changes[filename].append(line_range)

        prev_line = line

    return changes


def clang_tidy_on(
    clang_tidy_path: str,
    build_folder: pathlib.Path,
    file: str,
    file_diffs: list[tuple[int, int]],
    log_file,
):
    """
    Runs clang-tidy on a file with given file_diffs
    """

    # clang-tidy command succeeds if no errors were found.
    # Prints warnings on stdout.
    try:
        tidy_out, _ = bash_command_get_output(
            f"{clang_tidy_path} -p {build_folder} --format-style file --config-file .clang-tidy"
            f' --line-filter="[{{"name": "{file}", "lines": {file_diffs}}}]"'
            f" --extra-arg= {file}",
            click_file=log_file,
        )
        if tidy_out:
            log_warning(f"clang-tidy output: \n{tidy_out}", file=log_file)
    except BashCommandError as e:
        log_warning(
            f"clang-tidy failed: {file}, because:\n{e.stdout}{e.stderr}", file=log_file
        )


def clang_format_on(
    clang_format_path: str, file: str, file_diffs: list[tuple[int, int]], log_file
) -> bool:
    """
    Dry-run clang-format on a file with given file_diffs to test
    if it's properly formatted. Returns true if so, false otherwise.
    """

    # clang-format command succeeds always and returns data (possibly empty)
    lines = [f"--lines={start}:{stop}" for start, stop in file_diffs]
    format_out, format_err = bash_command_get_output(
        f"{clang_format_path}" " -style=file --dry-run" f" {' '.join(lines)} {file}",
        click_file=log_file,
    )

    # Print data returned by clang-format
    if format_out or format_err:
        log_warning(f"clang-format output: \n{format_out}{format_err}", file=log_file)
        return False
    return True


def run_linter_on(
    clang_tidy_path: str, clang_format_path: str, build_folder: pathlib.Path, file, diff
) -> tuple[str, bool]:
    clang_format_failed = False
    logs = ""
    if file.endswith(".hpp") or file.endswith(".cpp"):
        log_file = tempfile.TemporaryFile("w+")
        log_info(f"Linting: {file}", file=log_file)

        clang_tidy_on(clang_tidy_path, build_folder, file, diff, log_file)

        if not clang_format_on(clang_format_path, file, diff, log_file):
            clang_format_failed = True

        log_file.seek(0)
        logs = log_file.read()

    else:
        log_info(f"Skipping linting on: {file}")

    return logs, clang_format_failed


def simulate_cpp_linter(
    clang_tidy_path: str, clang_format_path: str, build: str, threads: int, branch: str
):
    build_folder = pathlib.Path(build)
    if not build_folder.exists():
        exit_with_error(f"Given build folder does not exist: {build_folder.absolute()}")

    file_diffs = get_diffs(branch)
    log_info(f"Found {file_diffs=}")

    clang_format_failed = False

    with ThreadPoolExecutor(max_workers=threads) as e:

        def call_linter(fd: tuple[str, list[tuple[int, int]]]):
            return run_linter_on(clang_tidy_path, clang_format_path, build_folder, *fd)

        results = e.map(call_linter, file_diffs.items())

        for logs, cf_failed in results:
            sys.stdout.write(logs)
            if cf_failed:
                clang_format_failed = True

    if clang_format_failed:
        to_format = get_input("Found formatting issues. Format the repo [Y/n]: ")
        if to_format.lower() in ["y", ""]:
            bash_command("./scripts/formatting/format_repo.sh")
