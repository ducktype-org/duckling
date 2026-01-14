from ..impl.setup_build import setup_build_impl
from .helpers import (
    build_system,
    build_dir,
    cxx_compiler,
    cc_compiler,
)
from ..impl.helpers import (
    default_compiler_from_ctx,
    default_linker_from_ctx,
    default_gcov_from_ctx,
)
from click import Choice, option, command


@command()
@build_dir(help="The name of the directory.")
@build_system(
    help="Build system to use",
)
# @TODO check if it is necessary to get compiler path from context
@cxx_compiler(
    help="A path to the C++ compiler to compile with",
    # this overrides the click.Option class to use the default_compiler_from_ctx
    # instead, so it can get ctx and infer and set the default value
    cls=default_compiler_from_ctx("cxx_compiler"),
)
@cc_compiler(
    help="A path to the C compiler to compile with",
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
@option(
    "-d",
    "--docs",
    help="Whether or not to build the docs.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--gcov-version",
    help="GCOV version that will be passed to find_program in CMAKE. If not specified, inferred from the C++ compiler.",
    default=None,
    cls=default_gcov_from_ctx(),
)
@option(
    "--linker",
    help="Specify the linker type to use. Auto-detects mold or lld if available.",
    default=None,
    cls=default_linker_from_ctx(),
)
@option(
    "-t",
    "--type",
    prompt="build type",
    help="The build type.",
    default="Debug",
    type=Choice(
        ["Dev", "DevDebug", "DevOpt", "Release", "ReleaseOpt", "Debug"],
        case_sensitive=False,
    ),
)
@option(
    "--shared_libs",
    help="Whether to use shared or static libraries.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--strip-symbol-information",
    help="Whether to strip all of symbol information from the binaries. It makes the binaries several times smaller, but practically prevents any debugging. Goes well with Release and non-Debug build types.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--disable-unity-compilation",
    help="Unity compilation (used only in parser) speeds up the build time significantly, but makes debugging harder (related linker errors lack information).",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--enable-link-time-optimization",
    help="Link time optimization (LTO) can improve performance by optimizing across translation units, but may make debugging more difficult. Requires a lot of resources.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--clang-for-builtins",
    help="Path to a custom Clang compiler for generating builtins. If not specified, auto-detected based on LLVM version.",
    default=None,
)
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_impl(*args, **kwargs)
