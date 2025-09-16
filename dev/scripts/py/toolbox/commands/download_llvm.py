from ..impl.download_llvm import download_llvm_impl
from .helpers import (
    llvm_version,
)
from click import command, option, Choice


@command()
@llvm_version(
    help="Version of LLVM release to compile, ex. 19.1.7",
)
@option(
    "-a",
    "--arch",
    prompt="Architecture",
    help="Architecture of the target machine",
    default="X64",
    type=Choice(["X64", "ARM64"], case_sensitive=False),
)
@option(
    "-c",
    "--confirm",
    prompt=(
        "From LLVM 19 onwards, the releases are compiled with unfavourable compile options, so it is recommended to either:\n"
        " - use the LLVM from your distribution (e.g. apt install llvm-19)\n"
        " - build LLVM from source (see `install-llvm` command)\n"
        "Do you want to continue with the download?"
    ),
    type=bool,
    default=True,
    is_flag=True,
)
@option(
    "-o",
    "--os",
    prompt="Operating system",
    help="Operating system of the target machine",
    default="linux",
    type=Choice(["Linux", "macOS", "Windows"], case_sensitive=False),
)
def download_llvm(*args, **kwargs):
    """Downloads the specified version of LLVM."""
    download_llvm_impl(*args, **kwargs)
