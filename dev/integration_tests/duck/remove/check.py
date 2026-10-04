import sys
from pathlib import Path

# Make ../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[1]))

from utilities import *

# There is a space after `source:`.
# If the checks fail it might be due to a subtle change in whitespaces added by yaml-edit.
expected_manifest_all = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  a:
    source: 
      path: ../a
dev-dependencies:
  b:
    source: 
      path: ../b
"""

expected_manifest_only_b = """metadata:
  name: foo
  version: '1.0.0'
dev-dependencies:
  b:
    source: 
      path: ../b
"""

expected_manifest_only_a = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  a:
    source: 
      path: ../a
"""

arg = sys.argv[1]
if arg == "all":
    expected_manifest = expected_manifest_all
elif arg == "only-a":
    expected_manifest = expected_manifest_only_a
elif arg == "only-b":
    expected_manifest = expected_manifest_only_b

manifest_path = project_root("foo") / "quackconfig.yaml"
manifest = manifest_path.read_text()

assert_eq(manifest, expected_manifest)