import sys
from pathlib import Path

# Make ../../../python/utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[3] / "python"))

from utilities import *

root = Path.cwd() / "foo"

artifacts = root / ".duck_build"
lock = artifacts / ".duck_lock"

check_empty_files([lock])
