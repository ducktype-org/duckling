import sys
from pathlib import Path

# Make ../../utilities.py import work
sys.path.append(str(Path(__file__).resolve().parents[2]))

from utilities import *

duck_home = default_duck_home()

cache = duck_home / "cache"
storage = duck_home / "storage"
locks = storage / "locks"

check_empty_files([cache / "fetcher.lock",
                   locks / "venv_data" / "foo",
                   locks / "venv_sync" / "foo",
                   locks / "clean.lock"])

check_empty_dirs([cache / "artifacts",
                  cache / "downloads"])

metadata_base = storage / "venv" / "foo"
metadata = metadata_base / "metadata"
old_metadata = metadata_base / "metadata.old"

check_files_equal(metadata, old_metadata)

check_venv_metadata(metadata)

expected_last_location = project_root("foo")

check_venv_last_location(file=metadata, expected=expected_last_location)

freeze = get_venv_freeze(metadata)

assert_eq(freeze["root"]["name"], "foo")
assert_eq(freeze["root"]["version"], "1.0.0")
assert_eq(freeze["root"]["features"], ["cycle"])
assert_eq(freeze["root"]["dependencies"], [])
assert_eq(freeze["dependencies"], [])
