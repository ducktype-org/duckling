from ..impl.install_llvm import install_llvm_impl
from .helpers import (
    build_system,
    cc_compiler,
    cxx_compiler,
    llvm_version,
)
from click import command, option, Choice


@command()
@build_system(
    help="Build system to use",
)
@cxx_compiler(
    help="A path to the C++ compiler to compile with",
    default="default",
    type=str,
)
@cc_compiler(
    help="A path to the C compiler to compile with",
    default="default",
    type=str,
)
@llvm_version(
    help="Version of LLVM release to compile, ex. 19.1.7",
)
@option(
    "-l",
    "--linker",
    prompt="Linker to use (using one of the newer linkers (lld/mold) will speed up the compilation)",
    help="Linker to use for building LLVM.",
    default="lld",
    type=Choice(["default", "lld", "mold"], case_sensitive=False),
)
@option(
    "-r",
    "--ram",
    "ram_gb",
    prompt="Available RAM (GB)",
    help="Amount of RAM available for linking (1 link job per 16GB)",
    default="16",
    type=str,
)
@option(
    "-s",
    "--source-dir",
    "source_dir_path",
    prompt="LLVM source directory",
    help="Directory where LLVM sources will be extracted",
    default="~/llvm",
    type=str,
)
@option(
    "-t",
    "--targets",
    prompt="LLVM targets to build (using 'all' will increase the build time about 3 times).",
    help="LLVM architecture targets to build.",
    default="Native",
    type=Choice(["Native", "X86", "all"], case_sensitive=False),
)
@option(
    "--use-old-build",
    prompt="Use old build directory",
    help="Whether or not to use the old build directory",
    default=False,
    type=bool,
)
def install_llvm(*args, **kwargs):
    """Compiles LLVM (including clang) from source with specified options.

    This command will download LLVM source code, build it with the specified options,
    and install it to the 'scripts/downloads/installed' directory.
    Important! It is advised to try to use the LLVM and clang from your
    distribution (e.g. apt install llvm-19 clang-19) first.
    """
    install_llvm_impl(*args, **kwargs)
