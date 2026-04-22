import hashlib
import sys
import filecmp
from pathlib import Path
import json
from typing import Any, Iterable


DEFAULT_SRC: Final[str] = """fun main() = {
    # !TODO: On macOS, builtin_output_string segfaults :^);
    # builtin_output_string("Hello, world!");
    return 0;
}
"""


def check_file_exists(file: Path) -> None:
    if not file.is_file():
        print(f"file {file} does not exist")
        sys.exit(1)


def check_dir_is_empty(directory: Path) -> None:
    if not directory.is_dir():
        print(f"not a directory: {directory}")
        sys.exit(1)
    if any(directory.iterdir()):
        print(f"directory {directory} is not empty")
        sys.exit(1)


def check_file_is_empty(file: Path) -> None:
    check_file_exists(file)
    if file.stat().st_size > 0:
        print(f"file {file} is not empty")
        sys.exit(1)


def check_files_equal(file1: Path, file2: Path) -> None:
    check_file_exists(file1)
    check_file_exists(file2)
    if not filecmp.cmp(file1, file2, shallow=False):
        print(f"files {file1} and {file2} are different")
        sys.exit(1)


def check_venv_metadata(file: Path) -> None:
    data, hash = file.read_text().rsplit("\n", 1)
    expected = hashlib.sha256(data.encode()).hexdigest()
    if hash != expected:
        print(f"sha256 mismatch in {file}: expected {expected}, got {hash}")
        sys.exit(1)


def default_duck_home() -> Path:
    return Path.cwd() / "duck_home"


def get_venv_data(file: Path) -> dict[str, Any]:
    data, _ = file.read_text().rsplit("\n", 1)
    return json.loads(data)


def get_venv_freeze(file: Path) -> dict[str, Any]:
    data = get_venv_data(file)
    return data["freeze"]


def check_venv_last_location(*, file: Path, expected: Path) -> None:
    data = get_venv_data(file)
    location = Path(data["last_known_directory"])
    if expected != location:
        print(f"expected last location to be {expected}, but instead is {location}")
        sys.exit(1)


def check_empty_files(files: Iterable[Path]) -> None:
    for file in files:
        check_file_is_empty(file)


def check_files_exist(files: Iterable[Path]) -> None:
    for file in files:
        check_file_exists(file)


def check_empty_dirs(dirs: Iterable[Path]) -> None:
    for dir in dirs:
        check_dir_is_empty(dir)


def get_exposed_freeze(root: Path) -> dict[str, Any]:
    file = root / "quackfreeze.json"
    data = file.read_text()
    return json.loads(data)


def assert_eq(lhs: Any, rhs: Any, msg: str | None = None) -> None:
    if lhs == rhs:
        return
    if msg != None:
        print(msg)
    print(f"`{lhs}` != `{rhs}`")
    sys.exit(1)


def check_src_from_root(root: Path):
    src = (root / "src" / "src.dmf").read_text()
    assert_eq(src, DEFAULT_SRC)
