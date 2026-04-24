from ..impl.setup_build import (setup_build_impl, LLVM_TOOLS)
from .helpers import (
    build_system,
    build_dir,
    cxx_compiler,
    cc_compiler,
)
from ..impl.helpers import (
    PromptForCoverageIfBuildNotOptimised,
    default_compiler_from_ctx,
    default_linker_from_ctx,
    default_gcov_from_ctx,
)
from ...llvm_tools import (LLVM_TOOLS, llvm_version_options)
from click import Choice, option, command, prompt

@command()
@build_dir(help="The name of the directory.")
@build_system(
    help="Build system to use",
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
    "--enable-jit",
    prompt="Enable JIT",
    help="Whether or not to enable JIT compilation.",
    type=bool,
    default=False,
    is_flag=True,
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
    # this skips the prompt if the build is optimised
    cls=PromptForCoverageIfBuildNotOptimised,
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
    "--allocator",
    help="Specify the allocator type to use. Currently supports None (default) and mimalloc.",
    type=Choice(["default", "mimalloc"], case_sensitive=False),
    default="default",
)
@option(
    "--shared_libs",
    help="Whether to use shared or static libraries.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "-i",
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
    help="Link time optimization (LTO) can improve performance by optimizing across translation units, but may make debugging more difficult. Requires Clang compiler and LLD linker (auto-configured).",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--clang-for-builtins",
    help="Path to a custom Clang compiler for generating builtins. If not specified, auto-detected based on LLVM version.",
    default=None,
)
@option(
    "--sanitizer",
    help="Enable a sanitizer. Choices: asan (AddressSanitizer), tsan (ThreadSanitizer), ubsan (UndefinedBehaviorSanitizer).",
    type=Choice(["asan", "tsan", "ubsan"], case_sensitive=False),
    default=None,
)
@option(
    "--use-replxx/--no-use-replxx",
    help="Whether to use replxx library for REPL frontend.",
    type=bool,
    default=True,
    is_flag=True,
)
@option(
    "--llvm-version",
    type=str,
    default="19",
    metavar="VERSION",
    help="Default version of llvm tools",
)
@llvm_version_options
def setup_build(*args, **kwargs):
    """Makes a build folder"""

    global_version = kwargs["llvm_version"]
    for tool in LLVM_TOOLS:
        if kwargs[tool.param()] == None:
            kwargs[tool.param()] = tool.default(global_version)

    setup_build_impl(
        *args,
        **kwargs,
    )
