import hashlib
import os
import subprocess
import sys
import filecmp
from pathlib import Path
import json
from typing import Any, Iterable, Final


DEFAULT_SRC: Final[str] = """import core.builtins.*;

fun main() = {
    return 0;
}
"""

DEFAULT_GITIGNORE: Final[str] = """.duck_build
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


def check_gitignore_from_root(root: Path):
    gitignore = (root / ".gitignore").read_text()
    assert_eq(gitignore, DEFAULT_GITIGNORE)


def check_no_gitignore_from_root(root: Path):
    gitignore = root / ".gitignore"
    assert_eq(gitignore.exists(), False)


def get_git_root(path: Path) -> Path | None:
    """Return the git repository root path or None if not in a git repo."""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--show-toplevel'],
            cwd=str(path),
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            return Path(result.stdout.strip())
    except (subprocess.SubprocessError, OSError):
        pass
    return None


def is_git_root(root: Path) -> bool:
    git_root = get_git_root(root)
    return git_root is not None and git_root == Path(root).resolve()


def check_is_git_root(root: Path):
    assert_eq(is_git_root(root), True)


def check_not_git_root(root: Path):
    assert_eq(is_git_root(root), False)


def artifacts_dir_for_root(root: Path) -> Path:
    return root / ".duck_build"


def artifacts_for_profile(root: Path, profile: str) -> Path:
    return root / profile


def unit_dir_name_for(name: str, version: str, source: Path | str) -> str:
    
    if isinstance(source, Path):
        source = str(source)
        source = f"local+file://{source}"
    hash = hashlib.sha256(source.encode()).hexdigest()
    return f"{name}-{version}-{hash}"

def deps_json_path_for_dep(dir: Path) -> Path:
    return dir / "deps.json"

def locks_path(dir: Path) -> Path:
    return dir / ".duck_lock"
