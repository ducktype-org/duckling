import sys
import shutil
from pathlib import Path

# Make ../../python/utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2] / "python"))

from utilities import *

root = Path.cwd() / "foo"

quackconfig = """metadata:
  name: foo
  version: '1.0.0'
"""

real_qp = (root / "quackconfig.yaml").read_text()
assert_eq(quackconfig, real_qp)

check_src_from_root(root)

shutil.rmtree(root)
