import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

expected_manifest = """metadata:
  name: foo
  version: 1.0.0
dependencies:
  a:
    source:
      path: ../a
"""

manifest_path = project_root("foo") / "quackconfig.yaml"
manifest = manifest_path.read_text()

assert_eq(manifest, expected_manifest)