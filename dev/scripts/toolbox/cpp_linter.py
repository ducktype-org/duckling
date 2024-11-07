import pathlib
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


def get_diffs():
    if new_unstaged_files := get_unstaged_new_files():
        log_warning(
            f"Files not in working tree, so not included in diff: [{', '.join(new_unstaged_files)}]"
        )

    diff_out, _ = bash_command_get_output(
        "git diff --merge-base origin/main -U0 --relative"
    )
    diff_lines = diff_out.splitlines()

    changes = {}

    prev_line = None
    filename = None
    for line in diff_lines:
        file_deleted = prev_line == '+++ /dev/null'

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


def simulate_cpp_linter(clang_tidy_path: str, clang_format_path: str, build: str):
    build_folder = pathlib.Path(build)
    if not build_folder.exists():
        exit_with_error(f"Given build folder does not exist: {build_folder.absolute()}")

    diffs = get_diffs()
    log_info(f"Found {diffs=}")

    clang_format_failed = False

    for file in diffs:
        if file.endswith(".hpp") or file.endswith(".cpp"):
            log_info(f"Running linting on: {file}")
            try:
                # clang-tidy command succeeds if no errors were found.
                # Prints warnings on stdout.
                tidy_out, _ = bash_command_get_output(
                    f"{clang_tidy_path} -p {build_folder} --format-style file"
                    f' --line-filter="[{{"name": "{file}", "lines": {diffs[file]}}}]"'
                    f" --extra-arg= {file}"
                )
                if tidy_out:
                    log_warning(f"clang-tidy output: \n{tidy_out}")

                # clang-format command succeeds always and returns data (possibly empty)
                lines = [f"--lines={start}:{stop}" for start, stop in diffs[file]]
                format_out, format_err = bash_command_get_output(
                    f"{clang_format_path}"
                    " -style=file --dry-run"
                    f" {' '.join(lines)} {file}"
                )

                # Parse data returned by clang-format
                if format_out or format_err:
                    log_warning(f"clang-format output: \n{format_out}{format_err}")
                    clang_format_failed = True

            except BashCommandError as e:
                log_warning(f"Failed: {file}, because:\n{e.stdout}{e.stderr}")
        else:
            log_info(f"Skipping linting on: {file}")

    if clang_format_failed:
        log_new_line()
        to_format = get_input(
            "Found formatting issues. Do you want to format the repo [Y/n]: "
        ).lower()
        log_new_line()
        if to_format == "y" or to_format == "":
            bash_command("./scripts/formatting/format_repo.sh")
