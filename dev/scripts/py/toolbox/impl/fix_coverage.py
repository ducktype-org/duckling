from pathlib import Path

from .helpers import (
    bash_command_get_output,
    exit_with_error,
    log_info,
    get_dev_directory,
)


def get_sources_of_gcno_file(gcno_path: Path) -> set[Path]:
    """
    Given a path to a .gcno file, get all the sources it refers to.
    """

    dev_dir = get_dev_directory()

    (strings_output, _) = bash_command_get_output(f"strings {gcno_path}")
    project_file_references = {
        line_path
        for line in strings_output.splitlines()
        # Filter only those strings which end in a source code extension.
        if (line_path := Path(str(line)).absolute())
        if line_path.suffix in {".hpp", ".cpp"}
        # Further filter only those strings which exist in our source code.
        if line_path.is_relative_to(dev_dir)
    }

    return project_file_references


def surgically_remove_coverage_files(build_path: Path):
    # Delete stale .gcno, .gcda, and .o files.
    gcno_paths = list(build_path.rglob("*.gcno"))
    gcno_path_count = len(gcno_paths)
    stale_count = 0
    processed_count = 0
    for gcno_path in gcno_paths:
        processed_count += 1
        print(f"\rProcessing file {processed_count}/{gcno_path_count}...", end="")

        # For each .gcno file, find its source files.
        source_files = get_sources_of_gcno_file(gcno_path)
        # If any of them don't exist...
        if any(not file.exists() for file in source_files):
            # Delete the .gcno file
            gcno_path.unlink()

            # Delete the matching .gcda and .o file if they exist
            gcda_path = gcno_path.with_suffix(".gcda")
            gcda_path.unlink(missing_ok=True)
            object_path = gcno_path.with_suffix(".o")
            object_path.unlink(missing_ok=True)

            print("\r", end="")
            log_info(f"Deleted stale coverage entry {gcno_path}.")
            stale_count += 1

    if stale_count:
        log_info(f"Deleted {stale_count} stale coverage entries.")


def nuke_extension(build_path: Path, ext: str):
    delete_count = 0
    for file_path in build_path.rglob(f"*.{ext}"):
        file_path.unlink()
        delete_count += 1
    log_info(f"Deleted {delete_count} .{ext} files.")


def nuke_coverage_files(build_path: Path):
    for ext in ("o", "gcno", "gcda"):
        nuke_extension(build_path, ext)


# See description of PR #1081 for solution explanation.
def fix_coverage_impl(build_dir: str, precision: str):
    # Preamble
    log_info("Running fix coverage...")
    build_path = Path(build_dir)
    if not build_path.exists():
        exit_with_error(f"Given build directory does not exist: {build_dir}.")

    # Remove stale coverage files
    if precision == "surgical":
        surgically_remove_coverage_files(build_path)
    else:
        nuke_coverage_files(build_path)
