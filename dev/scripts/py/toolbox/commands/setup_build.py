from ..impl.setup_build import setup_build_impl
from .helpers import (
    build_system,
    build_dir,
    cxx_compiler,
    cc_compiler,
)
from ..impl.helpers import (
    default_compiler_from_ctx,
)
from click import Choice, option, command


@command()
@build_dir(
    help="The name of the directory."
)
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
    prompt="Build docs",
    help="Whether or not to build the docs.",
    type=bool,
    default=True,
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
    "--shared_libs",
    prompt="Build shared libraries",
    help="Whether to use shared or static libraries.",
    type=bool,
    default=False,
    is_flag=True,
)
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_impl(*args, **kwargs)
