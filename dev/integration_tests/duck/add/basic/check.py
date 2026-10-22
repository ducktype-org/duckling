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

# There is a space after `source:`.
# If the checks fail it might be due to a subtle change in whitespaces added by yaml-edit.
expected_manifest_1 = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  a:
    source: 
      path: ../a
"""

expected_manifest_2 = """metadata:
  name: foo
  version: '1.0.0'
dev-dependencies:
  a:
    source: 
      path: ../a
"""

arg = sys.argv[1]
if arg == "1":
    expected_manifest = expected_manifest_1
elif arg == "2":
    expected_manifest = expected_manifest_2

manifest_path = project_root("foo") / "quackconfig.yaml"
manifest = manifest_path.read_text()

assert_eq(manifest, expected_manifest)