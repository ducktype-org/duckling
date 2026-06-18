import sys
import shutil
from pathlib import Path

# Make ../../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[3]))

from utilities import *

foo_path = Path.cwd() / "foo"
bar_path = Path.cwd() / "bar"

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
shutil.rmtree(artifacts_dir_for_root(foo_path))
