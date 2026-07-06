#!/usr/bin/env python3
"""
Helpers for running DIT test cases inside a temporary directory.

The directory is taken from $DIT_TMP_DIR, which tests define with the
`Env` config key (see `new_tmp_dir` in the root testconfig.yaml).

Subcommands:
    make [FILES...]  -- copy FILES (relative to the test's directory)
                        into the temporary directory, mirroring their
                        relative paths, and sweep stale directories
                        left over from past runs
    exec -- CMD...   -- run CMD inside the temporary directory
    clean            -- remove the temporary directory
"""
import os
import shutil
import sys
import time
from pathlib import Path

TMP_ROOT = Path("/tmp/dit")
# MacOS has a lot of weird symlinks.
# F.e. `/tmp` is a symlink to `/private/tmp`, and duck resolves paths, so I get a lot of mismatches on my local machine.
MACOS_WEIRD_TMP_ROOT = Path("/private/tmp/dit")
ALLOWED_TMP_ROOTS = [TMP_ROOT, MACOS_WEIRD_TMP_ROOT]
STALE_AGE_SECONDS = 24 * 60 * 60


def fail(msg: str):
    print(f"tmp_env.py: {msg}", file=sys.stderr)
    sys.exit(1)


def tmp_dir() -> Path:
    value = os.environ.get("DIT_TMP_DIR", "")
    if not value:
        fail("DIT_TMP_DIR is not set; define it with the `Env` config key")
    path = Path(value)
    is_under_valid_dir = any(root in path.parents for root in ALLOWED_TMP_ROOTS)
    if not is_under_valid_dir:
        valid_roots_string = " or ".join(str(x) for x in ALLOWED_TMP_ROOTS)
        fail(f"DIT_TMP_DIR ({value}) must live under {valid_roots_string}")
    return path


def sweep_stale():
    """
    Removes leftover directories of past runs. Directories of failed
    cases are intentionally kept around for debugging until they age out.
    """
    if not TMP_ROOT.exists():
        return
    now = time.time()
    for entry in TMP_ROOT.iterdir():
        try:
            if now - entry.stat().st_mtime > STALE_AGE_SECONDS:
                shutil.rmtree(entry, ignore_errors=True)
        except OSError:
            pass


def cmd_make(files: list[str]):
    path = tmp_dir()
    path.mkdir(parents=True, exist_ok=True)
    sweep_stale()
    for name in files:
        source = Path(name)
        if source.is_absolute():
            fail(f"only paths relative to the test's directory can be copied: {name}")
        if not source.exists():
            fail(f"{source} does not exist")
        destination = path / source
        destination.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, destination, dirs_exist_ok=True)
        else:
            shutil.copy2(source, destination)


def cmd_exec(argv: list[str]):
    if not argv:
        fail("no command given")
    path = tmp_dir()
    if not path.is_dir():
        fail(f"{path} does not exist; run `make` first")
    os.chdir(path)
    os.execvp("/bin/bash", ["/bin/bash", "-c", " ".join(argv)])


def cmd_clean():
    shutil.rmtree(tmp_dir(), ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        fail("usage: tmp_env.py make [FILES...] | exec -- CMD... | clean")
    match sys.argv[1]:
        case "make":
            cmd_make(sys.argv[2:])
        case "exec":
            args = sys.argv[2:]
            if args and args[0] == "--":
                args = args[1:]
            cmd_exec(args)
        case "clean":
            cmd_clean()
        case unknown:
            fail(f"unknown subcommand: {unknown}")


if __name__ == "__main__":
    main()
