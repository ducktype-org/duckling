import sys
import os
import re
from datetime import datetime
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

tmp_dir = os.environ["DIT_TMP_DIR"]

with open(Path(tmp_dir) / "output", "r") as f:
    output = f.read()

match = re.findall(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}", output)

last_access = datetime.strptime(match[0], "%Y-%m-%d %H:%M:%S")
last_modification = datetime.strptime(match[1], "%Y-%m-%d %H:%M:%S")
now = datetime.now()

assert((last_access - last_modification).total_seconds() >= 2)
assert((now - last_access).total_seconds() >= 2)
