# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import sys
from pathlib import Path

# Make ../../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[3]))

from utilities import *

foo_path = project_root("foo")
bar_path = project_root("bar")

version = "1.0.0"

profile = "dvm"

layout = artifacts_dir_for_root(foo_path)

layout = artifacts_for_profile(layout, profile)

foo_name = unit_dir_name_for("foo", version, foo_path)
bar_name = unit_dir_name_for("bar", version, bar_path)

foo_artifacts = layout / foo_name
bar_artifacts = layout / bar_name

assert not bar_artifacts.exists()

assert locks_path(foo_artifacts).exists()
check_file_is_empty(locks_path(foo_artifacts))

foo_deps = deps_json_path_for_dep(foo_artifacts)
assert foo_deps.exists()

text = foo_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{bar_name}",
      "name": "bar",
      "version": "1.0.0",
      "features": [],
      "path": "{str(bar_path)}/src",
      "dependencies": []
    }},
    {{
      "id": "{foo_name}",
      "name": "foo",
      "version": "1.0.0",
      "features": [],
      "path": "{str(foo_path)}/src",
      "dependencies": [
        {{
          "id": "{bar_name}"
        }}
      ]
    }}
  ],
  "tasks": [
    {{
      "package": "{foo_name}",
      "strategy": "dvm_exe",
      "output_file": "{str(layout / "foo.dbc")}"
    }}
  ]
}}"""

assert_eq(text, expected)
