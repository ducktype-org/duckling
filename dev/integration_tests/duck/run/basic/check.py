import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

foo_path = project_root("foo")

version = "1.0.0"

profile = sys.argv[1]
is_dvm = (profile == "my-profile") or (profile == "my-profile2")

layout = artifacts_dir_for_root(foo_path)

layout = artifacts_for_profile(layout, profile)

foo_name = unit_dir_name_for("foo", version, foo_path)

foo_artifacts = layout / foo_name

assert locks_path(foo_artifacts).exists()
check_file_is_empty(locks_path(foo_artifacts))

foo_deps = deps_json_path_for_dep(foo_artifacts)
assert foo_deps.exists()

text = foo_deps.read_text()

if not is_dvm:
    expected = f"""{{
  "packages": [
    {{
      "id": "{foo_name}",
      "name": "foo",
      "version": "1.0.0",
      "features": [],
      "path": "{str(foo_path)}/src",
      "dependencies": []
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
else:
    expected = f"""{{
  "packages": [
    {{
      "id": "{foo_name}",
      "name": "foo",
      "version": "1.0.0",
      "features": [],
      "path": "{str(foo_path)}/src",
      "dependencies": []
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
