# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.init import init_impl
from click import command


@command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc..."""
    init_impl()
