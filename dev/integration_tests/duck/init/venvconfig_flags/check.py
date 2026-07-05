import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

root = project_root("foo")

quackconfig = """metadata:
  name: foo
  version: '1.0.0'
"""

tested_flag = sys.argv[1]
match tested_flag:
  case "expose_freezefile":
    venv_config = """expose_freezefile: true
"""
  case "ephemeral":
    venv_config = """ephemeral: true
"""
  case "local_storage":
    venv_config = """local_storage: storage
"""
  case _:
    venv_config = ""

real_qp = (root / "quackconfig.yaml").read_text()
real_venv_config = (root / "venvconfig.yaml").read_text()
assert_eq(quackconfig, real_qp)
assert_eq(venv_config, real_venv_config)

check_src_from_root(root)
check_no_gitignore_from_root(root)

check_not_git_root(root)
