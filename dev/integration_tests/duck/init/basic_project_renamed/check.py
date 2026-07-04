import sys
import shutil
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

root = project_root("foo")

quackconfig = """metadata:
  name: bar
  version: '1.0.0'
"""

real_qp = (root / "quackconfig.yaml").read_text()
try:
  assert_eq(quackconfig, real_qp)

  check_src_from_root(root)
  check_no_gitignore_from_root(root)

  check_not_git_root(root)
finally:
  shutil.rmtree(root)
