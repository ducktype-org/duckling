# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import platform

import requests

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

# Used when the latest release can't be looked up.
FALLBACK_CCACHE_VERSION = "4.14.3"


def ccache_asset(version: str) -> str | None:
    """
    The upstream release asset of ccache `version` for the platform this is
    running on, or None when upstream publishes no binary for it.
    """
    # Keyed by (platform.system(), platform.machine()).
    # macOS ships one universal asset, hence the two entries pointing at it.
    assets: dict[tuple[str, str], str] = {
        ("Linux", "x86_64"): f"ccache-{version}-linux-x86_64-glibc.tar.xz",
        ("Linux", "aarch64"): f"ccache-{version}-linux-aarch64-glibc.tar.xz",
        ("Darwin", "arm64"): f"ccache-{version}-darwin.tar.gz",
        ("Darwin", "x86_64"): f"ccache-{version}-darwin.tar.gz",
    }
    return assets.get((platform.system(), platform.machine()))


def latest_ccache_version() -> str:
    """
    The latest ccache release with a binary for this platform, or
    FALLBACK_CCACHE_VERSION when GitHub can't tell.
    """
    try:
        res = requests.get(
            "https://api.github.com/repos/ccache/ccache/releases/latest", timeout=10
        )
        res.raise_for_status()
        release = res.json()
        version = release["tag_name"].removeprefix("v")
        published = {asset["name"] for asset in release["assets"]}
    except (requests.RequestException, KeyError, ValueError) as e:
        log_warning(
            f"Couldn't look up the latest ccache release ({e}), "
            f"using {FALLBACK_CCACHE_VERSION}."
        )
        return FALLBACK_CCACHE_VERSION

    if ccache_asset(version) not in published:
        log_warning(
            f"ccache {version} has no {ccache_asset(version)} asset, "
            f"using {FALLBACK_CCACHE_VERSION}."
        )
        return FALLBACK_CCACHE_VERSION

    return version


def ccache_file() -> InternetFile | None:
    """
    The latest ccache download for the platform this is running on, or None
    when upstream publishes no binary for it.
    """
    if ccache_asset(FALLBACK_CCACHE_VERSION) is None:
        return None

    version = latest_ccache_version()
    asset = ccache_asset(version)
    unpacked = asset.removesuffix(".tar.xz").removesuffix(".tar.gz")
    suffix = asset[len(unpacked) :]

    return InternetFile(
        f"scripts/downloads/ccache-{version}{suffix}",
        f"https://github.com/ccache/ccache/releases/download/v{version}/{asset}",
        after_download=[
            (callback_unTAR,),
            (callback_move, f"{unpacked}/ccache", "ccache"),
            (callback_remove, unpacked),
        ],
    )


def download_binaries_impl(force=False, single=False):
    log_info(
        f"Downloading binary files {'WITH force' if force else 'WITHOUT force'}..."
    )

    files_to_download = [file for file in (ccache_file(),) if file is not None]

    if not files_to_download:
        log_warning(
            f"No prebuilt ccache binary is published for {platform.system()} "
            f"{platform.machine()}; install a recent ccache on PATH yourself. "
            "Nothing to download."
        )
        return

    if single:
        log_info(f"Searching for file called '{single}'...")

        found = False
        for file in files_to_download:
            if single in file.resource_url:
                log_info(f"Found file: {file.resource_url}")
                file.download(force)
                found = True
                break
        if not found:
            exit_with_error(f"Couldn't find a file with '{single}' in resource url")

    else:
        log_info(f"Downloading all supported files")
        for file in files_to_download:
            file.download(force)

    log_info("Download done")
