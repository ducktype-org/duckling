#!/usr/bin/env python3
import click
from pathlib import Path

from toolbox.helpers import (
    bash_command,
    bash_command_get_output,
    exit_with_error,
    get_input,
    log_info,
    log_warning,
)

MAKE_PREPROCESSING_PREFIX = "Preprocessing CXX source to "


def run_preprocessor_impl(build_dir, cmake_path, source_file):
    log_warning(
        "This command will not work with ninja and might not work on non-linux system!"
    )

    cmake_dir = Path(build_dir) / cmake_path

    log_info(f"Running preprocessor for {source_file}.i in {cmake_dir}")

    out, _ = bash_command_get_output(f"make {source_file}.i", cwd=cmake_dir)

    if MAKE_PREPROCESSING_PREFIX not in out:
        log_warning("Something went wrong!")
        log_warning(f"Make output: {out}")
        return

    i_file = out[
        out.find(MAKE_PREPROCESSING_PREFIX) + len(MAKE_PREPROCESSING_PREFIX) :
    ].strip()

    output_file = cmake_dir / i_file

    log_info(f"Preprocessed file is at {output_file}")

    open_vscode = get_input("Open file in vscode? [Y/n]: ")
    if open_vscode.lower() in ["y", ""]:
        bash_command(f"code {output_file}")


@click.command()
@click.option(
    "-b",
    "--build-dir",
    prompt="build directory",
    help="The name of the build directory with enabled docs.",
    default="build",
)
@click.option(
    "-c",
    "--cmake-path",
    prompt="CMake file path",
    help="Path to a parent directory of cmake file defining the compilation of the file (for example the one defining add_library).",
)
@click.option(
    "-f",
    "--source-file",
    prompt="Source file path",
    help="File name to run preprocessor on, relative to cmake path (for example whatever was written in add_library).",
    type=str,
)
def run_preprocessor(*args, **kwargs):
    """Runs the preprocessor on the given file
    using CMake build system."""

    run_preprocessor_impl(*args, **kwargs)


if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    run_preprocessor()
