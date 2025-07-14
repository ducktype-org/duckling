from pathlib import Path
from os import cpu_count, path as os_path
from shutil import rmtree
from click import option, command, Choice

BUILD_SYSTEMS = Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

from .helpers import (
    bash_command,
    get_llvm_source_strings,
    log_info,
    log_new_line,
    exit_with_error,
)

from .internet_file import (
    InternetFile,
    callback_unTAR,
)

def install_llvm_impl(
    version,
    ram_gb,
    c_compiler,
    cxx_compiler,
    linker,
    build_tool,
    targets,
    source_dir_path,
    use_old_build,
):
    log_info("==========================")
    log_info("Building LLVM from source. This will take significant time!")
    log_info(
        "A single target build with lld and Ninja takes about 8 minutes on a 10-thread machine"
    )
    log_info(
        "You can also use LLVM from your distribution (e.g. apt install llvm-19) instead."
    )
    log_info("==========================")
    log_new_line()

    source_dir_path = Path(os_path.expanduser(source_dir_path))

    if not source_dir_path.exists():
        log_info(f"Creating LLVM source directory at {source_dir_path}")
        source_dir_path.mkdir(parents=True, exist_ok=True)

    link, downloaded_name, extracted_name, friendly_name = get_llvm_source_strings(
        version
    )
    extracted_name = Path(extracted_name)
    if not (source_dir_path / extracted_name).exists():
        # 1. Download to downloads directory
        download_dir = Path("scripts/downloads")
        downloaded_path = download_dir / Path(downloaded_name)

        llvm_file = InternetFile(
            str(downloaded_path),
            link,
            after_download=[
                (callback_unTAR,),
            ],
        )
        llvm_file.download()

        extracted_dir = download_dir / extracted_name
        bash_command(f"mv {extracted_dir} {source_dir_path}")
    else:
        log_info(
            f"LLVM source directory already exists at {source_dir_path / extracted_name}"
        )

    sources_path = source_dir_path / extracted_name
    install_dir = (Path("scripts/downloads") / friendly_name).absolute()

    # Calculate parallel link jobs based on RAM
    link_jobs = max(1, int(ram_gb) // 16)
    log_info(f"Using {link_jobs} parallel link jobs based on {ram_gb}GB RAM")

    # Create build directory
    build_dir = sources_path / "build"
    if not use_old_build:
        # remove old build directory if it exists
        if build_dir.exists():
            log_info(f"Removing old build directory: {build_dir}")
            rmtree(build_dir)

    if not build_dir.exists():
        build_dir.mkdir()

    # Configure LLVM build
    log_info("Configuring LLVM build...")

    # Build the cmake command
    cmake_cmd_parts = [
        f"cmake -S {sources_path}/llvm -B {build_dir}",
        f"-G '{build_tool}'",
        f"-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        f"-DLLVM_TARGETS_TO_BUILD={targets}",
        f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}",
    ]

    # Add linker option only if a specific linker is selected
    if linker != "default":
        cmake_cmd_parts.append(f"-DLLVM_USE_LINKER={linker}")

    if cxx_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_CXX_COMPILER={cxx_compiler}")
    if c_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_C_COMPILER={c_compiler}")

    cmake_command = " \\\n  ".join(cmake_cmd_parts)
    log_info(f"Running cmake command...")
    bash_command(cmake_command)

    # Build LLVM
    log_info("Building LLVM (this may take a while)...")
    bash_command(f"cmake --build {build_dir} -- -j{cpu_count() - 1}")

    # Install LLVM
    log_info("Installing LLVM...")
    bash_command(f"cmake --build {build_dir} --target install")

    log_info(f"LLVM {version} has been built and installed to {install_dir}")
    log_new_line()


@command()
@option(
    "-v",
    "--version",
    prompt="LLVM Version",
    help="Version of LLVM release to compile, ex. 19.1.7",
    default="19.1.7",
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
    "-x",
    "--cxx_compiler",
    prompt="C++ compiler path",
    help="A path to the C++ compiler to compile with",
    default="default",
    type=str,
)
@option(
    "-c",
    "--c-compiler",
    prompt="C compiler path",
    help="A path to the C compiler to compile with",
    default="default",
    type=str,
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
    "-b",
    "--build-tool",
    "build_tool",
    prompt="Build system",
    help="Build system to use",
    default="Ninja",
    type=BUILD_SYSTEMS,
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
    "-s",
    "--source-dir",
    "source_dir_path",
    prompt="LLVM source directory",
    help="Directory where LLVM sources will be extracted",
    default="~/llvm",
    type=str,
)
@option(
    "--use-old-build",
    prompt="Use old build directory",
    help="Whether or not to use the old build directory",
    default=False,
    type=bool,
)
def install_llvm(*args, **kwargs):
    """Compiles LLVM from source with specified options.

    This command will download LLVM source code, build it with the specified options,
    and install it to the 'scripts/downloads/installed' directory.
    Important! Is is advised to try to use the LLVM from your
    distribution (e.g. apt install llvm-19) first.
    """
    install_llvm_impl(*args, **kwargs)

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    install_llvm()