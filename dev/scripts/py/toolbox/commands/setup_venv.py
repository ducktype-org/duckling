# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.setup_venv import setup_venv_impl
from click import command


@command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_impl()
