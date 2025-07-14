from pathlib import Path
from shutil import rmtree
from click import option, Choice, command

from .helpers import (
    bash_command,
    log_info,
    with_venv,
    check_if_compilers_are_compatible,
    supports_cmake_linker_type,
    should_add_linker_flags,
    default_compiler_from_ctx,
    exit_with_error,
)

BUILD_SYSTEMS = Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)


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
):

    check_if_compilers_are_compatible(cxx_compiler, cc_compiler)

    bld = Path(build_dir)
    if bld.exists():
        # Delete old cache
        try:
            (bld / Path("CMakeCache.txt")).unlink()
            rmtree(bld / Path("CMakeFiles"))
        except FileNotFoundError:
            pass
    cmd_parts = [
        f"cmake",
        f'-G "{build_system}"',
        f"-B {build_dir}",
        f"-D CMAKE_BUILD_TYPE={type}",
        f"-D BUILD_DOCS={'ON' if docs else 'OFF'}",
        f"-D CMAKE_CXX_COMPILER={cxx_compiler}",
        f"-D CMAKE_C_COMPILER={cc_compiler}",
        f"-D GCOV_VERSION={gcov_version}",
        f"-D USE_CCACHE={'ON' if ccache else 'OFF'}",
        f"-D ENABLE_COVERAGE={'true' if coverage else 'false'}",
    ]
    if should_add_linker_flags(linker):
        if supports_cmake_linker_type():
            cmd_parts.append(f"-D CMAKE_LINKER_TYPE={linker.upper()}")
        else:
            cmd_parts.append(f'-D CMAKE_CXX_FLAGS="-fuse-ld={linker.lower()}"')
    
    cmd = " ".join(cmd_parts)

    log_info("Setting up a build folder...")
    if docs or coverage:
        with_venv(cmd)
    else:
        bash_command(cmd)


@command()
@option(
    "-b",
    "--build_dir",
    prompt="Build dir name",
    help="The name of the directory.",
    default="build",
)
@option(
    "-s",
    "--build-system",
    prompt="Build system",
    help="The build system to use",
    type=BUILD_SYSTEMS,
    default="Ninja",
)
@option(
    "-t",
    "--type",
    prompt="build type",
    help="The build type.",
    default="debug",
    type=Choice(
        ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], case_sensitive=False
    ),
)
@option(
    "-d",
    "--docs",
    prompt="Build docs",
    help="Whether or not to build the docs.",
    type=bool,
    default=True,
    is_flag=True,
)
@option(
    "-x",
    "--cxx-compiler",
    prompt="C++ compiler path",
    help="A path to the C++ complier to compile with",
    # this overrides the click.Option class to use the default_compiler_from_ctx
    # instead, so it can get ctx and infer and set the default value
    cls=default_compiler_from_ctx("cxx_compiler"),
)
@option(
    "-c",
    "--cc-compiler",
    prompt="C compiler path",
    help="A path to the C complier to compile with",
    # this overrides the click.Option class to use the default_compiler_from_ctx
    # instead, so it can get ctx and infer and set the default value
    cls=default_compiler_from_ctx("cc_compiler"),
)
@option(
    "--ccache",
    prompt="Use ccache",
    help="Whether or not to use ccache.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--coverage",
    prompt="Enable coverage",
    help="Whether or not to enable coverage",
    type=bool,
    default=False,
    is_flag=True,
)
# @TODO: make it prompt only for cov-build (see https://click.palletsprojects.com/en/stable/options/#callbacks-and-eager-options)
@option(
    "--gcov-version",
    prompt="GCOV version",
    help="GCOV version that will be passed to find_program in CMAKE",
    default="gcov-14",
)
@option(
    "--linker",
    prompt="Linker",
    help="Specify the linker type to use",
    default="default",
)
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_impl(*args, **kwargs)

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    setup_build()
