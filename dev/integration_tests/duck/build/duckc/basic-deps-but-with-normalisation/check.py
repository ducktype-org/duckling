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

root_path = project_root("my-cool-root")
dep_path = project_root("my-cool-dep")
dep2_path = project_root("my-cool-dep2")

version = "1.0.0"

profile = "dev"

layout = artifacts_dir_for_root(root_path)

layout = artifacts_for_profile(layout, profile)

root_name = unit_dir_name_for("my-cool-root", version, root_path)
dep_name = unit_dir_name_for("my-cool-dep", version, dep_path)
dep2_name = unit_dir_name_for("my-cool-dep2", version, dep2_path)

root_artifacts = layout / root_name
dep_artifacts = layout / dep_name
dep2_artifacts = layout / dep2_name

assert locks_path(dep_artifacts).exists()
check_file_is_empty(locks_path(dep_artifacts))

dep_deps = deps_json_path_for_dep(dep_artifacts)
assert dep_deps.exists()

text = dep_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{dep_name}",
      "name": "my_cool_dep",
      "version": "1.0.0",
      "features": [],
      "path": "{str(dep_path)}/src",
      "dependencies": []
    }}
  ],
  "tasks": [
    {{
      "package": "{dep_name}",
      "strategy": "lib",
      "output_file": "{str(layout / dep_name / f"{dep_name}.a")}"
    }}
  ]
}}"""

assert_eq(text, expected)

assert locks_path(dep2_artifacts).exists()
check_file_is_empty(locks_path(dep2_artifacts))

dep2_deps = deps_json_path_for_dep(dep2_artifacts)
assert dep2_deps.exists()

text = dep2_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{dep2_name}",
      "name": "my_cool_dep2",
      "version": "1.0.0",
      "features": [],
      "path": "{str(dep2_path)}/src",
      "dependencies": []
    }}
  ],
  "tasks": [
    {{
      "package": "{dep2_name}",
      "strategy": "lib",
      "output_file": "{str(layout / dep2_name / f"{dep2_name}.a")}"
    }}
  ]
}}"""

assert_eq(text, expected)

assert locks_path(root_artifacts).exists()
check_file_is_empty(locks_path(root_artifacts))

foo_deps = deps_json_path_for_dep(root_artifacts)
assert foo_deps.exists()

text = foo_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{root_name}",
      "name": "my_cool_root",
      "version": "1.0.0",
      "features": [],
      "path": "{str(root_path)}/src",
      "dependencies": [
        {{
          "id": "{dep_name}"
        }},
        {{
          "id": "{dep2_name}",
          "alias": "my_cool_dep_but_aliased"
        }}
      ]
    }},
    {{
      "id": "{dep_name}",
      "name": "my_cool_dep",
      "version": "1.0.0",
      "features": [],
      "path": "{str(dep_path)}/src",
      "dependencies": []
    }},
    {{
      "id": "{dep2_name}",
      "name": "my_cool_dep2",
      "version": "1.0.0",
      "features": [],
      "path": "{str(dep2_path)}/src",
      "dependencies": []
    }}
  ],
  "tasks": [
    {{
      "package": "{root_name}",
      "strategy": "native",
      "output_file": "{str(layout / "my-cool-root")}",
      "linking_options": [
        "{str(dep_artifacts / f"{dep_name}.a")}",
        "{str(dep2_artifacts / f"{dep2_name}.a")}"
      ]
    }}
  ]
}}"""

assert_eq(text, expected)
