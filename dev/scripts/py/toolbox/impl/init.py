# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from .helpers import (
    bash_command,
    log_info,
    log_new_line,
)
from .download_binaries import download_binaries_impl
from .setup_venv import setup_venv_impl


def init_impl():
    log_info("Initializing the REPO!...")
    log_new_line()

    log_info("Initializing git submodules...")
    bash_command("git submodule update --init")
    log_new_line()

    setup_venv_impl()
    log_new_line()

    download_binaries_impl(False)
    log_new_line()
