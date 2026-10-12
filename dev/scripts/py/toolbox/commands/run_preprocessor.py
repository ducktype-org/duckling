# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.run_preprocessor import run_preprocessor_impl
from .helpers import build_dir
from click import command, option


@command()
@build_dir(
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
)
@option(
    "-c",
    "--cmake-path",
    prompt="CMake file path",
    help="Path to a parent directory of cmake file defining the compilation of the file (for example the one defining add_library).",
)
@option(
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
