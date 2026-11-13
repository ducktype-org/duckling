# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import platform
import shutil
import subprocess
import tempfile
from pathlib import Path

from .helpers import (
    log_good,
    log_info,
    log_warning,
)

"""
System libraries that LLVM links against and that the toolbox does not
install: `setup-build` fails in CMake when they are missing. Each entry is
(name, CMake lines that set `DUCK_FOUND` to a true value when it is found).
"""
_DEPENDENCIES: list[tuple[str, str]] = [
    ("zlib", "find_package(ZLIB QUIET)\nset(DUCK_FOUND ${ZLIB_FOUND})"),
    (
        # The same lookup as LLVM's own Findzstd.cmake.
        "zstd",
        "find_path(DUCK_ZSTD_INCLUDE NAMES zstd.h)\n"
        "find_library(DUCK_ZSTD_LIBRARY NAMES zstd zstd_static)\n"
        "set(DUCK_FOUND FALSE)\n"
        "if(DUCK_ZSTD_INCLUDE AND DUCK_ZSTD_LIBRARY)\n"
        "  set(DUCK_FOUND TRUE)\n"
        "endif()",
    ),
]

_MARKER = "DUCK_SYSTEM_DEP"

"""Package that provides each library, per `platform.system()`."""
_PACKAGES: dict[str, dict[str, str]] = {
    "Linux": {"zlib": "zlib1g-dev", "zstd": "libzstd-dev"},
    "Darwin": {"zstd": "zstd"},
}


def _install_hint(missing: list[str]) -> str | None:
    system = platform.system()
    packages = _PACKAGES.get(system, {})
    if system == "Linux":
        names = [packages[name] for name in missing if name in packages]
        return f"sudo apt install {' '.join(names)}  (Debian/Ubuntu)" if names else None
    if system == "Darwin":
        steps = []
        names = [packages[name] for name in missing if name in packages]
        if names:
            steps.append(f"brew install {' '.join(names)}")
        if "zlib" in missing:
            # zlib comes with the macOS SDK.
            steps.append("xcode-select --install")
        return " && ".join(steps) if steps else None
    return None


def _probe_script() -> str:
    lines = [
        "cmake_minimum_required(VERSION 3.25)",
        # A language is needed: CMake learns the library directories (the
        # multiarch subdirectory on Debian, the SDK on macOS) from the compiler.
        "project(duckling_system_deps C)",
    ]
    for name, lookup in _DEPENDENCIES:
        lines += [
            "unset(DUCK_FOUND)",
            lookup,
            "if(DUCK_FOUND)",
            f'  message(STATUS "{_MARKER} {name} 1")',
            "else()",
            f'  message(STATUS "{_MARKER} {name} 0")',
            "endif()",
        ]
    return "\n".join(lines) + "\n"


def find_missing_system_deps() -> list[str] | None:
    """
    Returns the names of the system libraries CMake cannot find, or None when
    the check could not run (no `cmake` or C compiler, or the probe failed).

    The libraries are looked up by configuring a throwaway CMake project, so
    they are found exactly where `setup-build` would find them (the system
    prefixes, `CMAKE_PREFIX_PATH`, the macOS SDK).
    """
    cmake = shutil.which("cmake")
    if cmake is None:
        return None

    with tempfile.TemporaryDirectory(prefix="duckling-deps-") as tmp:
        Path(tmp, "CMakeLists.txt").write_text(_probe_script())
        result = subprocess.run(
            [cmake, "-S", tmp, "-B", str(Path(tmp, "build"))],
            capture_output=True,
            text=True,
        )
    if result.returncode != 0:
        return None

    found: dict[str, bool] = {}
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[1] == _MARKER:
            found[parts[2]] = parts[3] == "1"

    if len(found) != len(_DEPENDENCIES):
        return None
    return [name for name, _ in _DEPENDENCIES if not found[name]]


def check_system_deps_impl() -> bool:
    """
    Checks that the system libraries the build needs (zlib, zstd) are
    installed, and logs how to install the missing ones. Never fails the
    caller: returns False when a library is missing or the check cannot run.
    """
    names = ", ".join(name for name, _ in _DEPENDENCIES)
    log_info(f"Checking system libraries ({names})...")

    missing = find_missing_system_deps()
    if missing is None:
        log_warning(
            "Could not check the system libraries: `cmake` or the C compiler "
            "is missing, or the CMake probe failed."
        )
        return False

    if missing:
        log_warning(f"Missing system libraries: {', '.join(missing)}.")
        hint = _install_hint(missing)
        if hint is not None:
            log_warning(f"Install them with: {hint}")
        log_warning("Without them, `setup-build` fails when CMake looks for LLVM.")
        return False

    log_good(f"System libraries found: {names}.")
    return True
