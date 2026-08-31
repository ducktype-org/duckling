import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

expected_manifest_1 = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  b:
    source:
      name: a
      path: ../a
"""

expected_manifest_21 = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  a:
    source:
      path: ../a
  b:
    source:
      name: a
      path: ../a
"""

expected_manifest_22 = """metadata:
  name: foo
  version: '1.0.0'
dependencies:
  b:
    source:
      name: a
      path: ../a
  a:
    source:
      path: ../a
"""

manifest_path = project_root("foo") / "quackconfig.yaml"
manifest = manifest_path.read_text()


arg = sys.argv[1]
if arg == "1":
    assert_eq(manifest, expected_manifest_1)
elif arg == "2":
    assert(manifest == expected_manifest_21 or manifest == expected_manifest_22)