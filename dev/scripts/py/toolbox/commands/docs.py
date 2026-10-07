# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.docs import docs_impl
from .helpers import (
    build_dir,
)
from click import command


@command()
@build_dir(
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
)
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_impl(*args, **kwargs)
