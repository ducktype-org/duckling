import sys
from pathlib import Path

# Make ../../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[3]))

from utilities import *

tmp1_path = project_root("tmp1")

expected_subfolders = int(sys.argv[1])

build_folder = artifacts_dir_for_root(tmp1_path)

check_num_subfolders(build_folder, expected_subfolders)
