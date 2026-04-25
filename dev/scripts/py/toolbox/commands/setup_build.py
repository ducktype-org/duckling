from ..impl.setup_build import setup_build_impl
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
from click import Choice, option, command, prompt


def configure_presets(ctx, param, value):
    if param.name != "preset":
        raise click.BadParameter("Preset configuration can only be applied to the --preset option.")

    preset_map = {}

    if value is None:
        return
    elif value == "ReleasePreset":
        preset_map = {
            "type": "DevOpt", # Note that we use DevOpt for now, as we prefer to have controlled panics, until they are not rare enough
            "coverage": False,
            "docs": False,
            "allocator": "default", # until we are 100% sure other allocators work well
            "shared_libs": False,
            "strip_symbol_information": True,
            "embed_assets": True,
            "build_static_icu": True, # we want to have a static ICU in release to make the binary portable
        }
    elif value == "MaxPerformancePreset":
        preset_map = {
            "type": "ReleaseOpt",
            "coverage": False,
            "docs": False,
            "allocator": "mimalloc",
            "shared_libs": False,
            "strip_symbol_information": True,
        }
    else:
        raise ValueError(f"Unknown preset: {value}")
    
    ctx.default_map = preset_map

@command()
@option( 
    # The presets logic is implemented based on an article you can find here: https://jwodder.github.io/kbits/posts/click-config/
    # Note: Presets should correctly override default values provided by our custom option classes (set in cls parameters),
    # but it's best to test it per-case, since Python allows to do quite about anything, and there might be some edge cases.
    "--preset",
    help         = "Use a predefined set of default option values for a specific build configuration.",
    type         = Choice(["ReleasePreset", "MaxPerformancePreset"], case_sensitive=False),
    callback     = configure_presets,
    is_eager     = True,
    expose_value = False,
)
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
    "--ccache/--no-ccache",
    prompt="Use ccache",
    help="Whether or not to use ccache.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--coverage/--no-coverage",
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
    "--docs/--no-docs",
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
    "--shared_libs/--no-shared_libs",
    help="Whether to use shared or static libraries.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "-i",
    "--strip-symbol-information/--no-strip-symbol-information",
    help="Whether to strip all of symbol information from the binaries. It makes the binaries several times smaller, but practically prevents any debugging. Goes well with Release and non-Debug build types.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--disable-unity-compilation/--no-disable-unity-compilation",
    help="Unity compilation (used only in parser) speeds up the build time significantly, but makes debugging harder (related linker errors lack information).",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--enable-link-time-optimization/--no-enable-link-time-optimization",
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
    "--enable-jit/--no-enable-jit",
    prompt="Enable JIT",
    help="Whether or not to enable JIT compilation.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--embed-assets/--no-embed-assets",
    help="Whether to embed assets into the binary. This makes the binary portable. Currently the assets include diagnostic message templates.",
    type=bool,
    default=False,
    is_flag=True,
)
@option(
    "--build-static-icu/--no-build-static-icu",
    help="Forces building and linking against a custom-built static version of ICU.",
    type=bool,
    default=False,
    is_flag=True,
)
def setup_build(*args, **kwargs):
    """Makes a build folder"""

    enable_jit = kwargs.pop("enable_jit")

    if enable_jit:
        llvm_linker = prompt(
            "Path to LLVM linker, llvm-link",
        )
        opt_path = prompt(
            "Path to LLVM optimizer, opt",
        )
    else:
        llvm_linker = None
        opt_path = None

    kwargs.update(
        {
            "enable_jit": enable_jit,
            "llvm_linker": llvm_linker,
            "opt_path": opt_path,
        }
    )

    setup_build_impl(
        *args,
        **kwargs,
    )
