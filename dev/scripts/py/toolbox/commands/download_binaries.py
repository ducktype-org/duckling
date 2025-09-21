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
