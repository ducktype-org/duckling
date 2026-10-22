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
baz_path = project_root("baz")

version = "1.0.0"

profile = "dev"

layout = artifacts_dir_for_root(foo_path)

layout = artifacts_for_profile(layout, profile)

foo_name = unit_dir_name_for("foo", version, foo_path)
bar_name = unit_dir_name_for("bar", version, bar_path)
baz_name = unit_dir_name_for("baz", version, bar_path)

foo_artifacts = layout / foo_name
bar_artifacts = layout / bar_name
baz_artifacts = layout / baz_name

assert locks_path(bar_artifacts).exists()
check_file_is_empty(locks_path(bar_artifacts))

bar_deps = deps_json_path_for_dep(bar_artifacts)
assert bar_deps.exists()

text = bar_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{bar_name}",
      "name": "bar",
      "version": "1.0.0",
      "features": [],
      "path": "{str(bar_path)}/src",
      "dependencies": [
        {{
          "id": "{baz_name}"
        }}
      ]
    }},
    {{
      "id": "{baz_name}",
      "name": "baz",
      "version": "1.0.0",
      "features": [],
      "path": "{str(baz_path)}/src",
      "dependencies": [
        {{
          "id": "{foo_name}"
        }}
      ]
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
      "package": "{bar_name}",
      "strategy": "lib",
      "output_file": "{str(layout / bar_name / f"{bar_name}.a")}"
    }}
  ]
}}"""

assert_eq(text, expected)

assert locks_path(baz_artifacts).exists()
check_file_is_empty(locks_path(baz_artifacts))

baz_deps = deps_json_path_for_dep(baz_artifacts)
assert baz_deps.exists()

text = baz_deps.read_text()

expected = f"""{{
  "packages": [
    {{
      "id": "{baz_name}",
      "name": "baz",
      "version": "1.0.0",
      "features": [],
      "path": "{str(baz_path)}/src",
      "dependencies": [
        {{
          "id": "{foo_name}"
        }}
      ]
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
    }},
    {{
      "id": "{bar_name}",
      "name": "bar",
      "version": "1.0.0",
      "features": [],
      "path": "{str(bar_path)}/src",
      "dependencies": [
        {{
          "id": "{baz_name}"
        }}
      ]
    }}
  ],
  "tasks": [
    {{
      "package": "{baz_name}",
      "strategy": "lib",
      "output_file": "{str(layout / baz_name / f"{baz_name}.a")}"
    }}
  ]
}}"""

assert_eq(text, expected)

foo_deps = deps_json_path_for_dep(foo_artifacts)
assert foo_deps.exists()

assert locks_path(foo_artifacts).exists()
check_file_is_empty(locks_path(foo_artifacts))

text = foo_deps.read_text()

expected = f"""{{
  "packages": [
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
    }},
    {{
      "id": "{bar_name}",
      "name": "bar",
      "version": "1.0.0",
      "features": [],
      "path": "{str(bar_path)}/src",
      "dependencies": [
        {{
          "id": "{baz_name}"
        }}
      ]
    }},
    {{
      "id": "{baz_name}",
      "name": "baz",
      "version": "1.0.0",
      "features": [],
      "path": "{str(baz_path)}/src",
      "dependencies": [
        {{
          "id": "{foo_name}"
        }}
      ]
    }}
  ],
  "tasks": [
    {{
      "package": "{foo_name}",
      "strategy": "native",
      "output_file": "{str(layout / "foo")}"
    }}
  ]
}}"""

assert_eq(text, expected)
