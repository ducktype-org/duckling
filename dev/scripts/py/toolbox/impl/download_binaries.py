# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import json
import pathlib
import platform

from .helpers import (
    exit_with_error,
    log_info,
    log_warning,
)

from .internet_file import (
    InternetFile,
    callback_move,
    callback_remove,
    callback_unTAR,
)

# Also read by CMake, which rejects any other ccache version.
SHIPPED_BINARIES_FILE = (
    pathlib.Path(__file__).resolve().parents[3] / "shipped_binaries.json"
)
SHIPPED_VERSIONS: dict[str, str] = json.loads(SHIPPED_BINARIES_FILE.read_text())
CCACHE_VERSION = SHIPPED_VERSIONS["ccache"]


def ccache_file() -> InternetFile | None:
    """
    The ccache download for the platform this is running on, or None when
    upstream publishes no binary for it.
    """
    # The upstream release asset for each platform we can build on, keyed by
    # (platform.system(), platform.machine()).
    # macOS ships one universal asset, hence the two entries pointing at it.
    assets: dict[tuple[str, str], str] = {
        ("Linux", "x86_64"): f"ccache-{CCACHE_VERSION}-linux-x86_64-glibc.tar.xz",
        ("Linux", "aarch64"): f"ccache-{CCACHE_VERSION}-linux-aarch64-glibc.tar.xz",
        ("Darwin", "arm64"): f"ccache-{CCACHE_VERSION}-darwin.tar.gz",
        ("Darwin", "x86_64"): f"ccache-{CCACHE_VERSION}-darwin.tar.gz",
    }

    asset = assets.get((platform.system(), platform.machine()))
    if asset is None:
        return None

    unpacked = asset.removesuffix(".tar.xz").removesuffix(".tar.gz")
    suffix = asset[len(unpacked) :]

    return InternetFile(
        f"scripts/downloads/ccache{suffix}",
        f"https://github.com/ccache/ccache/releases/download/v{CCACHE_VERSION}/{asset}",
        after_download=[
            (callback_unTAR,),
            (callback_move, f"{unpacked}/ccache", "ccache"),
            (callback_remove, unpacked),
        ],
    )


FILES_TO_DOWNLOAD: list[InternetFile] = [
    file for file in (ccache_file(),) if file is not None
]


def download_binaries_impl(force=False, single=False):
    log_info(
        f"Downloading binary files {'WITH force' if force else 'WITHOUT force'}..."
    )

    if not FILES_TO_DOWNLOAD:
        log_warning(
            f"No prebuilt ccache binary is published for {platform.system()} "
            f"{platform.machine()}; install ccache from your package manager "
            "instead (the build finds it on PATH). Nothing to download."
        )
        return

    if single:
        log_info(f"Searching for file called '{single}'...")

        found = False
        for file in FILES_TO_DOWNLOAD:
            if single in file.resource_url:
                log_info(f"Found file: {file.resource_url}")
                file.download(force)
                found = True
                break
        if not found:
            exit_with_error(f"Couldn't find a file with '{single}' in resource url")

    else:
        log_info(f"Downloading all supported files")
        for file in FILES_TO_DOWNLOAD:
            file.download(force)

    log_info("Download done")
