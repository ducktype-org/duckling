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

root = project_root("foo")

quackconfig = """metadata:
  name: foo
  version: '1.0.0'
"""

tested_flag = sys.argv[1]
match tested_flag:
  case "expose-freezefile":
    quackconfig += """venv:
  expose-freezefile: true
"""
  case "ephemeral":
    quackconfig += """venv:
  ephemeral: true
"""
  case "local-storage":
    quackconfig += """venv:
  storage-path: storage
"""
  case _:
    venv_config = ""

real_qp = (root / "quackconfig.yaml").read_text()
assert_eq(quackconfig, real_qp)

check_src_from_root(root)
check_no_gitignore_from_root(root)

check_not_git_root(root)
