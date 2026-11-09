# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import http
import http.client
import os
import pathlib
import sys
import time
import urllib.parse

import requests
import math

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
    log_warning,
)

# GitHub answers release downloads with a 5xx from time to time (most often 504), and those clear up
# within seconds. So we try a few times before failing. 429 is GitHub's rate limit.
RETRY_STATUS_CODES = {429, 500, 502, 503, 504}
DOWNLOAD_ATTEMPTS = 5
RETRY_BASE_DELAY_SECONDS = 5

# Hosts that get the token from `GITHUB_TOKEN` (or `GH_TOKEN`). Authenticated requests get a higher
# limit than anonymous ones. `requests` drops the header itself when GitHub redirects the download
# to another host (its CDN), so the token never leaves github.com.
GITHUB_HOSTS = {"github.com", "api.github.com"}


def github_auth_headers(url: str) -> dict[str, str]:
    """
    The `Authorization` header for a github.com URL when `GITHUB_TOKEN` or
    `GH_TOKEN` is set, otherwise no header (an anonymous request).
    """
    if urllib.parse.urlparse(url).hostname not in GITHUB_HOSTS:
        return {}
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    return {"Authorization": f"Bearer {token}"} if token else {}


class InternetFile:

    def __init__(self, path, resource_url, auth=(), after_download=()):
        self.path = pathlib.Path(path)
        self.resource_url = resource_url
        self.auth = auth
        self.after_download_callbacks = after_download
        self.parent_dir = self.path.parent.absolute()

    def download(self, force=False):
        if force or not self.path.exists():
            try:
                log_info(f"Downloading {self.path}...")
                res = self._get_with_retries()

                if res.status_code != 200:
                    exit_with_error(
                        f"Cannot download: {self.resource_url}, status_code = {res.status_code}, meaning: {http.client.responses[res.status_code]}"
                    )

                with open(self.path, "wb") as f:
                    total_length = int(res.headers.get("content-length"))
                    total_length_mib = None
                    written_format_base = None
                    display_progress_bar = (
                        total_length is not None
                    )  # content length header exists
                    bytes_to_mib = lambda x: round(x / 1024 / 1024, 2)

                    if display_progress_bar:
                        total_length_mib = bytes_to_mib(total_length)

                        # This is a special python format string, so we can add
                        # padding with spaces: {:>X}, where X is the amount of
                        # space which will be occupied (spaces + content), and it
                        # will be aligned to the right (>).
                        #
                        # floor(log10(x)) is for counting decimal digits and '+3' is leaving
                        # space for '.XY'.
                        written_format_base = (
                            "{:>"
                            + str(math.floor(math.log10((total_length_mib))) + 3)
                            + "}"
                        )
                    else:
                        log_info(
                            f"Cannot display a progress bar during downloading of {self.path}..."
                        )

                    written = 0
                    for chunk in res.iter_content(chunk_size=8192):
                        f.write(chunk)

                        if display_progress_bar:
                            written += len(chunk)
                            written_mib = bytes_to_mib(written)
                            written_mib_padded = written_format_base.format(written_mib)

                            # Force a carriage return and rewrite the line
                            sys.stdout.write(
                                f"\r{self.path}: {written_mib_padded}/{total_length_mib} MiB"
                            )
                            sys.stdout.flush()

                    if display_progress_bar:
                        sys.stdout.write("\n")
                        sys.stdout.flush()

                log_info(f"Done downloading {self.path} from {self.resource_url}")

                for callback in self.after_download_callbacks:
                    callback[0](self, *callback[1:])

            except requests.exceptions.HTTPError as e:
                exit_with_error(e)
        else:
            log_info(f"File {self.path} already exits. Skipped.")

    def _get_with_retries(self) -> requests.Response:
        """
        GETs the resource, retrying on connection errors and on the status codes
        in RETRY_STATUS_CODES. Returns the last response; raises the last
        connection error when no attempt got a response at all.
        """
        # An explicit `auth` (user, password) wins over the GitHub token.
        headers = {} if self.auth else github_auth_headers(self.resource_url)
        for attempt in range(1, DOWNLOAD_ATTEMPTS + 1):
            last_attempt = attempt == DOWNLOAD_ATTEMPTS
            try:
                res = requests.get(
                    self.resource_url,
                    stream=True,
                    auth=self.auth,
                    headers=headers,
                    timeout=60,
                )
            except (requests.ConnectionError, requests.Timeout) as e:
                if last_attempt:
                    raise
                problem = type(e).__name__
            else:
                if res.status_code not in RETRY_STATUS_CODES or last_attempt:
                    return res
                problem = f"status_code = {res.status_code}"
                res.close()

            delay = RETRY_BASE_DELAY_SECONDS * 2 ** (attempt - 1)
            log_warning(
                f"Download of {self.resource_url} failed ({problem}), "
                f"retrying in {delay} s (attempt {attempt}/{DOWNLOAD_ATTEMPTS})..."
            )
            time.sleep(delay)


def callback_unTAR(res: InternetFile):
    log_info(f"Untarring {res.path}...")
    bash_command(f"tar -xf {res.path} --directory={res.parent_dir}")


def callback_remove(res: InternetFile, file_path: str):
    log_info(f"Removing {file_path}...")
    bash_command(f"rm -r {res.parent_dir / file_path}")


def callback_move(res: InternetFile, path_from: str, path_to: str):
    log_info(f"Moving {res.parent_dir / path_from} to {res.parent_dir / path_to}...")
    bash_command(f"mv {res.parent_dir / path_from} {res.parent_dir / path_to}")


def callback_chmod(res: InternetFile, path, mode):
    log_info(f"Chmoding {res.parent_dir / path} to {mode}...")
    bash_command(f"chmod {mode} {res.parent_dir / path}")
