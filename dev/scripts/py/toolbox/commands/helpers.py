from click import option, Choice

BUILD_SYSTEMS = Choice(["Ninja", "Unix Makefiles"], case_sensitive=False)

def create_option(*def_arg, **def_kwargs):
    """
    Creates a customizable `click.option` decorator with predefined defaults,
    allowing overrides at the point of use.
    """
    def specialize_option(*arg, **kwargs):
        return option(
            *(arg or def_arg),
            **{**def_kwargs, **kwargs}
        )
    
    return specialize_option

# HERE DEFINE REPEATING FLAGS

def all_flag(*args, **kwargs):
    return create_option(
        "-a",
        "--all",
        "all",
        is_flag=True,
        type=bool,
        default=False,
    )(*args, **kwargs)

def branch(*args, **kwargs):
    return create_option(
        "-r",
        "--branch",
        "branch",
        type=str,
        default="origin/main",
    )(*args, **kwargs)

def build_dir(*args, **kwargs):
    return create_option(
        "-b",
        "--build-dir",
        "build_dir",
        prompt="build directory",
        type=str,
        default="build",
    )(*args, **kwargs)

def build_system(*args, **kwargs):
    return create_option(
        "-b",
        "--build-system",
        "build_system",
        prompt="Build system",
        default="Ninja",
        type=BUILD_SYSTEMS,
    )(*args, **kwargs)

def cc_compiler(*args, **kwargs):
    return create_option(
        "-c",
        "--cc-compiler",
        "cc_compiler",
        prompt="C compiler path",
    )(*args, **kwargs)

def clang_format(*args, **kwargs):
    return create_option(
        "-f",
        "--format",
        "clang_format_path",
        prompt="clang-format path",
        type=str,
        default="clang-format-19",
    )(*args, **kwargs)

def clang_tidy(*args, **kwargs):
    return create_option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        type=str,
        default="clang-tidy-19",
    )(*args, **kwargs)

def cxx_compiler(*args, **kwargs):
    return create_option(
        "-x",
        "--cxx-compiler",
        "cxx_compiler",
        prompt="C++ compiler path",
    )(*args, **kwargs)

def llvm_version(*args, **kwargs):
    return create_option(
        "-v",
        "--llvm-version",
        "llvm_version",
        prompt="LLVM Version",
        type=str,
        default="19.1.7",
    )(*args, **kwargs)

def no_merge_base(*args, **kwargs):
    return create_option(
        "--no-merge-base",
        is_flag=True,
        type=bool,
        default=False,
    )(*args, **kwargs)

def thread_count(*args, **kwargs):
    return create_option(
        "-j",
        "--thread-count",
        "thread_count",
        prompt="Number of threads to use",
    )(*args, **kwargs)

def verbose(*args, **kwargs):
    return create_option(
        "-v",
        "--verbose",
        "verbose",
        is_flag=True,
        default=False,
    )(*args, **kwargs)