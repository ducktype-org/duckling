from ..impl.setup_build import setup_build_impl
from ..impl.helpers import (
    default_compiler_from_ctx,
    BUILD_SYSTEMS,
)
from click import Choice, option, command

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