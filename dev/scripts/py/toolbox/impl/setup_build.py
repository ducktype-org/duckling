from pathlib import Path
from shutil import rmtree

from .helpers import (
    bash_command,
    log_info,
    with_venv,
    check_if_compilers_are_compatible,
    supports_cmake_linker_type,
    should_add_linker_flags,
    exit_with_error,
)

from ...jit.llvm_tools import LLVM_TOOLS

def setup_build_impl(
    build_dir,
    build_system,
    type,
    docs,
    cxx_compiler,
    cc_compiler,
    gcov_version,
    ccache,
    coverage,
    linker,
    allocator,
    shared_libs,
    strip_symbol_information,
    disable_unity_compilation,
    enable_link_time_optimization,
    clang_for_builtins,
    sanitizer,
    use_replxx,
    enable_jit,
    llvm_tools_list,
    embed_assets,
    build_static_icu,
    fixed_std_path,
):

    check_if_compilers_are_compatible(cxx_compiler, cc_compiler)

    # If LTO is enabled, ensure we're using Clang and LLD
    if enable_link_time_optimization:
        if "clang++" not in cxx_compiler:
            exit_with_error(
                f"Link-time optimization (LTO) requires Clang compiler. "
                f"Current compiler: {cxx_compiler}. "
                f"Please use --cxx-compiler to specify a Clang compiler (e.g., clang++-19)."
            )
        if linker != "lld":
            log_info("LTO enabled: Setting linker to lld")
            linker = "lld"

    bld = Path(build_dir)
    if bld.exists():
        # Delete old cache
        try:
            (bld / Path("CMakeCache.txt")).unlink()
            rmtree(bld / Path("CMakeFiles"))
        except FileNotFoundError:
            pass

    # This is needed for CMake File API.
    # Used by e.g. `./toolbox.py test`.
    bash_command(f"mkdir -p {build_dir}/.cmake/api/v1/query/")
    bash_command(f"touch {build_dir}/.cmake/api/v1/query/codemodel-v2")

    cmd_parts = [
        f"cmake",
        f"-B {build_dir}",
        f'-G "{build_system}"',
        f"-D CMAKE_BUILD_TYPE={type}",
        f"-D BUILD_DOCS={'ON' if docs else 'OFF'}",
        f"-D CMAKE_CXX_COMPILER={cxx_compiler}",
        f"-D CMAKE_C_COMPILER={cc_compiler}",
        f"-D USE_CCACHE={'ON' if ccache else 'OFF'}",
        f"-D ENABLE_COVERAGE={'true' if coverage else 'false'}",
        f"-D ALLOCATOR={allocator.upper()}",
        f"-D BUILD_SHARED_LIBS={'ON' if shared_libs else 'OFF'}",
        f"-D STRIP_SYMBOL_INFORMATION={'ON' if strip_symbol_information else 'OFF'}",
        f"-D DISABLE_UNITY_COMPILATION={'ON' if disable_unity_compilation else 'OFF'}",
        f"-D ENABLE_LINK_TIME_OPTIMIZATION={'ON' if enable_link_time_optimization else 'OFF'}",
        f"-D JIT_ENABLED={'ON' if enable_jit else 'OFF'}",
        f"-D USE_REPLXX={'ON' if use_replxx else 'OFF'}",
        *[
            f"-D {tool.macro()}={llvm_tools_list[tool.param()]}"
            for tool in LLVM_TOOLS
        ],
        f"-D EMBED_ASSETS={'ON' if embed_assets else 'OFF'}",
        f"-D BUILD_STATIC_ICU={'ON' if build_static_icu else 'OFF'}",
        f"-D USE_FIXED_STD_PATH={'ON' if fixed_std_path else 'OFF'}",
    ]
    if sanitizer:
        cmd_parts.append(f"-D SANITIZER={sanitizer.upper()}")
    if coverage:
        cmd_parts.append(f"-D GCOV_VERSION={gcov_version}")
    if clang_for_builtins:
        cmd_parts.append(f"-D CLANG_BIN={clang_for_builtins}")

    if should_add_linker_flags(linker):
        if supports_cmake_linker_type():
            cmd_parts.append(f"-D CMAKE_LINKER_TYPE={linker.upper()}")
        else:
            cmd_parts.append(f'-D CMAKE_EXE_LINKER_FLAGS="-fuse-ld={linker.lower()}"')

    cmd = " ".join(cmd_parts)

    log_info("Setting up a build folder...")
    if docs or coverage or enable_jit:
        with_venv(cmd)
    else:
        bash_command(cmd)
