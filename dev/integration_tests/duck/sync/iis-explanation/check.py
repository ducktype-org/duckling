# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

explanation = Path("/tmp/qp_iis_expl.txt")
assert explanation.exists()

text = explanation.read_text()

expected = """The following constraints are mutually infeasible:

no realization of dependency `bar` for root package found; package must not be present in the solution

root package must be in the solution"""

assert_eq(text, expected)