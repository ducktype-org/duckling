from click import option, Choice
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

def impl(version, os, arch):
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

    link, downloaded, extracted, friendly = get_llvm_strings(version, os, arch)

    llvm_file = InternetFile(
        downloaded,
        link,
        after_download=[
            (callback_unTAR,),
        ],
    )
    llvm_file.download()
    
def confirm(func):
    return option(
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
	)(func)

def version(func):
    return option(
		"-v",
		"--version",
		prompt="LLVM Version",
		help="Version of LLVM release, ex. 19.1.4",
		default="19.1.7",
	)(func)

def os(func):
	return option(
		"-o",
		"--os",
		prompt="Operating system",
		help="Operating system of the target machine",
		default="linux",
		type=Choice(["Linux", "macOS", "Windows"], case_sensitive=False),
	)(func)

def architecture(func):
	return option(
		"-a",
		"--arch",
		prompt="Architecture",
		help="Architecture of the target machine",
		default="X64",
		type=Choice(["X64", "ARM64"], case_sensitive=False),
	)(func)