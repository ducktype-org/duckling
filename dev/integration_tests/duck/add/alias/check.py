import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

expected_manifest_1 = """metadata:
  name: foo
  version: 1.0.0
dependencies:
  b:
    source:
      path: ../a
      name: a
"""

expected_manifest_2 = """metadata:
  name: foo
  version: 1.0.0
dependencies:
  a:
    source:
      path: ../a
  b:
    source:
      path: ../a
      name: a
"""

arg = sys.argv[1]
if arg == "1":
    expected_manifest = expected_manifest_1
elif arg == "2":
    expected_manifest = expected_manifest_2

manifest_path = project_root("foo") / "quackconfig.yaml"
manifest = manifest_path.read_text()

assert_eq(manifest, expected_manifest)