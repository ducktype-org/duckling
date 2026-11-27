# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from click import group

from .ir_stats import ir_stats


@group()
def vm():
    """
    DVM development tools.

    Run `./toolbox.py vm COMMAND --help` for the details of a command.
    """
    pass


vm.add_command(ir_stats)
