import sys
from pathlib import Path

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

check_not_git_root(root)
