from .helpers import (
    get_llvm_strings,
    log_info,
    log_new_line,
    log_warning,
)

from .internet_file import (
    InternetFile,
    callback_unTAR,
)


def download_llvm_impl(llvm_version, os, arch):
    log_info("==========================")
    log_warning(
        "Downloading LLVM may or may not work, depending on a presence of compiled binaries listed here: https://github.com/llvm/llvm-project/releases/"
    )
    log_new_line()
    log_info("- A note on binaries -")
    log_info("Volunteers make binaries for the LLVM project, which will be uploaded")
    log_info("when they have had time to test and build these binaries. They might")
    log_info("not be available directly or not at all for each release. We suggest")
    log_info("you use the binaries from your distribution or build your own if you")
    log_info("rely on a specific platform or configuration.")
    log_info("==========================")
    log_new_line()

    link, downloaded, extracted, friendly = get_llvm_strings(llvm_version, os, arch)

    llvm_file = InternetFile(
        downloaded,
        link,
        after_download=[
            (callback_unTAR,),
        ],
    )
    llvm_file.download()
