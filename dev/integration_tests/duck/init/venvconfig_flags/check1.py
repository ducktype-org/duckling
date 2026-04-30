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

venv_config = """expose_freezefile: true
"""

real_qp = (root / "quackconfig.yaml").read_text()
real_venv_config = (root / "venvconfig.yaml").read_text()
try:
  assert_eq(quackconfig, real_qp)
  assert_eq(venv_config, real_venv_config)

  check_src_from_root(root)
  check_gitignore_from_root(root)

  assert(is_git_root(root))
finally:
  shutil.rmtree(root)
