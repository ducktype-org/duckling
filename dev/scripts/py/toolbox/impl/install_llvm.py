from pathlib import Path
from os import path as os_path
from shutil import rmtree

from ..commands.helpers import get_cpu_count
from .helpers import (
    bash_command,
    get_llvm_source_strings,
    log_info,
    log_new_line,
)

from .internet_file import (
    InternetFile,
    callback_unTAR,
)


def install_llvm_impl(
    llvm_version,
    ram_gb,
    cc_compiler,
    cxx_compiler,
    linker,
    build_system,
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
        llvm_version
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
        f"-G '{build_system}'",
        f"-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        f"-DLLVM_TARGETS_TO_BUILD={targets}",
        f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}",
        f"-DLLVM_ENABLE_PROJECTS=\"clang\"",
        # These options are required for VM's JIT to link properly
        f"-DLLVM_ENABLE_RTTI=ON",
        f"-DLLVM_ENABLE_EH=ON",
    ]

    # Add linker option only if a specific linker is selected
    if linker != "default":
        cmake_cmd_parts.append(f"-DLLVM_USE_LINKER={linker}")

    if cxx_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_CXX_COMPILER={cxx_compiler}")
    if cc_compiler != "default":
        cmake_cmd_parts.append(f"-DCMAKE_C_COMPILER={cc_compiler}")

    cmake_command = " \\\n  ".join(cmake_cmd_parts)
    log_info(f"Running cmake command...")
    bash_command(cmake_command)

    # Build LLVM
    log_info("Building LLVM (this may take a while)...")
    # get_cpu_count() already carries the +1 that CI uses for build parallelism; the -1
    # keeps this call's original intent of leaving the machine one core to breathe.
    jobs = max(1, get_cpu_count() - 1)
    bash_command(f"cmake --build {build_dir} -- -j{jobs}")

    # Install LLVM
    log_info("Installing LLVM...")
    bash_command(f"cmake --build {build_dir} --target install")

    log_info(f"LLVM {llvm_version} has been built and installed to {install_dir}")
    log_new_line()
