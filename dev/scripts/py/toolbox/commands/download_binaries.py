# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.download_binaries import download_binaries_impl
from click import command, option


@command()
@option(
    "-f",
    "--force",
    help="Whether or not to force the download of files that already exits",
    is_flag=True,
    type=bool,
    default=False,
)
@option(
    "-s",
    "--single",
    help="Download a single file, that is fuzzily named as passed in this flag",
    type=str,
    default="",
)
def download_binaries(*args, **kwargs):
    """Downloads necessary binary files from the internet"""
    download_binaries_impl(*args, **kwargs)
