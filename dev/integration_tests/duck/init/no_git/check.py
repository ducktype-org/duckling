import subprocess
import sys
import shutil
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

root = Path.cwd() / "foo"

quackconfig = """metadata:
  name: foo
  version: '1.0.0'
"""

real_qp = (root / "quackconfig.yaml").read_text()
assert_eq(quackconfig, real_qp)

check_src_from_root(root)
check_no_gitignore_from_root(root)

# Check this is not a git repository
cmd = "cd " + str(root) + " && git rev-parse --show-toplevel > tmp"
os.system(cmd)
real_toplevel = (root / "tmp").read_text()
assert(not (real_toplevel == str(root)))
