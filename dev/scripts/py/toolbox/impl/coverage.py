from pathlib import Path

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
    get_cpu_count,
)


def nuke_extension(build_path: Path, ext: str):
    delete_count = 0
    for file_path in build_path.rglob(f"*.{ext}"):
        file_path.unlink()
        delete_count += 1
    log_info(f"Deleted {delete_count} .{ext} files.")


def get_source_of_gcno_file(build_path: Path, gcno_path: Path) -> Path | None:
    """
    Given a path to a .gcno file, try to resolve the matching source file.
    Assumes the source root is the part of the path before 'CMakeFiles/<target>.dir/'.
    """

    # Find the split point, which is the occurrence of `/CMakeFiles/` in the path.
    parts = gcno_path.parts
    cmake_idx = parts.index("CMakeFiles")

    # Take everything after the build directory and before `/CMakeFiles/` as the source root.
    source_root = Path(*parts[:cmake_idx]).relative_to(build_path)

    # Get the relative source path (everything after the <target>.dir/)
    # Skip: `/CMakeFiles/<target>.dir/`
    # Strip `.gcno` extension to obtain a `.cpp` suffix.
    relative_source_path = Path(*parts[cmake_idx + 2:]).with_suffix('')

    # Combine the paths to get the source `.cpp` file, and return.
    source_path = source_root / relative_source_path
    return source_path if source_path.exists() else None


def surgically_remove_coverage_files(build_path: Path):
    # Delete stale .gcno, .gcda, and .o files.
    stale_count = 0

    for gcno_file in build_path.rglob("*.gcno"):
        # For each .gcno file, find its source file.
        source_file = get_source_of_gcno_file(build_path, gcno_file)
        # If it doesn't exist...
        if source_file is None:
            # Delete the .gcno file
            gcno_file.unlink()

            # Delete the matching .gcda and .o file if it exists
            gcda_file = gcno_file.with_suffix(".gcda")
            gcda_file.unlink(missing_ok=True)
            object_file = gcno_file.with_suffix(".o")
            object_file.unlink(missing_ok=True)

            log_info(f"Deleted stale coverage entry {gcno_file}.")
            stale_count += 1

    if stale_count:
        log_info(f"Deleted {stale_count} stale coverage entries.")


def nuke_coverage_files(build_path: Path):
    for ext in ("o", "gcno", "gcda"):
        nuke_extension(build_path, ext)


def coverage_impl(build_dir, thread_count, precision):
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

    # Remove stale coverage files
    if precision == "nuke":
        nuke_coverage_files(build_path)
    else:
        surgically_remove_coverage_files(build_path)

    # Rebuild, retest, recompute coverage
    bash_command(f"cmake --build {build_dir} {thread_option} -- build_all_tests")
    bash_command(f"cmake --build {build_dir} {thread_option} -- test")

    bash_command(f"cmake --build {build_dir} -- coverage")

    # Open coverage report
    bash_command("xdg-open coverage/index.html", cwd=build_dir)
