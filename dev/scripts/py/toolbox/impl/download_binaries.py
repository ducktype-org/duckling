from .helpers import (
    exit_with_error,
    log_info,
)

from .internet_file import (
    InternetFile,
    callback_move,
    callback_remove,
    callback_unTAR,
)

FILES_TO_DOWNLOAD: list[InternetFile] = [
    InternetFile(
        "scripts/downloads/ccache.tar.xz",
        "https://github.com/ccache/ccache/releases/download/v4.9.1/ccache-4.9.1-linux-x86_64.tar.xz",
        after_download=[
            (callback_unTAR,),
            (callback_move, "ccache-4.9.1-linux-x86_64/ccache", "ccache"),
            (callback_remove, "ccache-4.9.1-linux-x86_64"),
        ],
    ),
]

def download_binaries_impl(force=False, single=False):
    log_info(
        f"Downloading binary files {'WITH force' if force else 'WITHOUT force'}..."
    )

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