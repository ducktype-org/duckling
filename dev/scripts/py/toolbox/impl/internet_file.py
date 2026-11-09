# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import http
import http.client
import pathlib
import sys

import requests
import math

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
)


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
                res = requests.get(self.resource_url, stream=True, auth=self.auth)

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
