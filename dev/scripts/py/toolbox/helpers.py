import pathlib
import subprocess as sp
import sys
import re

import click

from packaging import version
import shutil


def with_venv(cmd):
    if not pathlib.Path(".venv").exists():
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    bash_command(f"source .venv/bin/activate && {cmd}")


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


class BashCommandError(Exception):
    def __init__(
        self, command: str, exit_code, stdout, stderr, at: pathlib.Path = None
    ):
        super().__init__(
            f"\n\tBash command `{command}` {f'\n\texecuted at `{at.absolute()}` ' if at else ''}\n\thas failed with an exit code: {exit_code}, because:\n"
            + f"[STDOUT]:{"\n" + stdout if stdout else ""}\n"
            + f"[STDERR]:{"\n" + stderr if stderr else ""}"
        )
        self.command = command
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr
        self.at = at


def exec_bash_command(
    command: str,
    cwd: str | pathlib.Path,
    capture_output=False,
    input: bytes | None = None,
    exitcode=0,
    dry: bool = False,
    verbose: bool = False,
    decode: bool = True,
    log_to_file=sys.stdout,
) -> tuple[bytes, bytes]:
    """
    This is the lowest level access to calling a bash command in toolbox.
    """
    if isinstance(cwd, str):
        cwd = pathlib.Path(cwd)
    if dry or verbose:
        command = replace_special(command)
        log_bash(f'cd "{cwd.absolute()}" && {command}', file=log_to_file)
        if dry:
            return bytes(), bytes()

    proc = sp.Popen(
        ["/bin/bash", "-c", command],
        cwd=cwd,
        stdin=sp.PIPE if input else None,
        stdout=sp.PIPE if capture_output else None,
        stderr=sp.PIPE if capture_output else None,
    )
    stdout, stderr = proc.communicate(input=input)

    status = proc.wait()

    # Check if there's a need for decoding
    if status != exitcode or decode:
        if stdout is not None:
            stdout = stdout.decode("UTF-8")
        if stderr is not None:
            stderr = stderr.decode("UTF-8")

    if status != exitcode:
        raise BashCommandError(command, status, stdout, stderr, at=cwd)

    return stdout, stderr


def bash_command(command: str, cwd: str = ".", log_file=sys.stdout):
    log_bash(command, file=log_file)
    exec_bash_command(command=command, cwd=cwd, log_to_file=log_file)


def bash_command_get_output(command: str, cwd: str = ".", log_file=sys.stdout):
    return exec_bash_command(
        command=command, cwd=cwd, capture_output=True, log_to_file=log_file
    )


def click_log(prefix, msg, fg, bold=False, nl=True, file=sys.stdout):
    click.echo(
        click.style(f"[{prefix}]: {msg}", fg=fg, bold=bold),
        color=True,
        nl=nl,
        file=file,
    )


def log_info(msg, file=sys.stdout):
    click_log("INFO", msg, fg="yellow", file=file)


def log_bash(msg, file=sys.stdout):
    click_log("BASH", msg, fg="bright_cyan", file=file)


def log_warning(msg, file=sys.stdout):
    click_log("WARNING", msg, fg="magenta", bold=True, file=file)


def get_input(msg, nl=False):
    click_log("INPUT", msg, fg="blue", nl=nl)
    return input()


def log_new_line(file=sys.stdout):
    click.echo("", file=file)


def abort_if_false(ctx, param, value):
    if not value:
        ctx.abort()


def get_llvm_strings(version, os, arch) -> tuple[str, str, str, str]:
    """
    Takes `version`, `os` and `arch` params and returns a tuple[link_to_download, downloaded_file, extracted_file, friendly_name]
    """
    name = ""
    if int(version.split(".")[0]) >= 19:
        name = f"LLVM-{version}-{os}-{arch}"

        return (
            f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/{name}.tar.xz",
            f"scripts/downloads/llvm_{version}_{os}_{arch}.tar.xz",
            name,
            f"lib_llvm_{version}_{os}_{arch}",
        )

    else:
        # This branch is legacy, for LLVM 18 and lower.
        if os != "Linux":
            exit_with_error(
                f"This configuration is not supported: {version=}, {arch=} {os=}. Visit: https://github.com/llvm/llvm-project/releases/"
            )

        if arch == "X64":
            name = f"clang+llvm-{version}-x86_64-linux-gnu-ubuntu-18.04"
        else:
            name = f"clang+llvm-{version}-aarch64-linux-gnu"

        return (
            f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/{name}.tar.xz",
            f"scripts/downloads/llvm_{version}_{arch}.tar.xz",
            name,
            f"llvm_lib_{version}_{arch}",
        )


def get_llvm_source_strings(version) -> tuple[str, str, str, str]:
    """
    Takes `version` param and returns a tuple[link_to_download, downloaded_file, extracted_file, friendly_name]
    for LLVM source code.
    """
    return (
        f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{version}/llvm-project-{version}.src.tar.xz",
        f"llvm-project-{version}.src.tar.xz",
        f"llvm-project-{version}.src",
        f"llvm_lib_{version}_native",
    )


def replace_special(command: str) -> str:
    """
    Replaces special chars like '\n' or '\t' to '\\n' and '\\t'.

    This helps with readability of eg. program output.
    """
    command = command.replace("\n", "\\n")
    command = command.replace("\t", "\\t")
    command = command.replace("\r", "\\r")
    return command


def truncate_str(string, max_len=10, surround="`"):
    """
    Truncate a string and adds `surround` char around the string. If string is longer than `max_len` does string[:max_len] + surround + '...'.
    """
    return f"{surround}{string[:max_len] + (f'{surround}...' if len(string) > max_len else surround)}"


# this class overrides the click.Option class, so it can get ctx
# and infer and set the default value from other options
def default_compiler_from_ctx(default_name):

    class OptionDefaultFromCtx(click.Option):

        def get_default(self, ctx, call=True):
            if default_name == "cc_compiler":
                self.default = infer_cc_compiler(ctx)
            elif default_name == "cxx_compiler":
                self.default = infer_cxx_compiler(ctx)
            return super(OptionDefaultFromCtx, self).get_default(ctx, call)

    return OptionDefaultFromCtx


def infer_cc_compiler(ctx):
    """Infer the default C compiler from the C++ compiler"""
    cc_compiler = ctx.params.get("cc_compiler")
    cxx_compiler = ctx.params.get("cxx_compiler")

    if not cc_compiler and cxx_compiler:
        if "clang++" in cxx_compiler:
            cc_compiler = cxx_compiler.replace("clang++", "clang")
        elif "g++" in cxx_compiler:
            cc_compiler = cxx_compiler.replace("g++", "gcc")
        elif "icpx" in cxx_compiler:
            cc_compiler = cxx_compiler.replace("icpx", "icx")
        elif "icpc" in cxx_compiler:
            cc_compiler = cxx_compiler.replace("icpc", "icc")
        else:
            cc_compiler = "gcc"
    else:
        cc_compiler = "gcc"

    return cc_compiler


def infer_cxx_compiler(ctx):
    """Infer the default C++ compiler from the C compiler"""
    cc_compiler = ctx.params.get("cc_compiler")
    cxx_compiler = ctx.params.get("cxx_compiler")

    if not cxx_compiler and cc_compiler:
        if "clang" in cc_compiler:
            cxx_compiler = cc_compiler.replace("clang", "clang++")
        elif "gcc" in cc_compiler:
            cxx_compiler = cc_compiler.replace("gcc", "g++")
        elif "icx" in cc_compiler:
            cxx_compiler = cc_compiler.replace("icx", "icpx")
        elif "icc" in cc_compiler:
            cxx_compiler = cc_compiler.replace("icc", "icpc")
        else:
            cxx_compiler = "g++"
    else:
        cxx_compiler = "g++"

    return cxx_compiler


def get_program_version(prog):
    version_out = sp.run([prog, "--version"], capture_output=True, text=True)
    version_info = version_out.stdout.splitlines()[0]
    match = re.search(r"(\d+(\.\d+)+)", version_info)
    return match.group(0) if match else None


def check_if_compilers_are_compatible(cxx_compiler, cc_compiler):
    if cxx_compiler is None or cc_compiler is None:
        exit_with_error("Couldn't get the compilers")

    if (
        ("clang" in cc_compiler and "clang++" not in cxx_compiler)
        or ("gcc" in cc_compiler and "clang++" in cxx_compiler)
        or ("gcc" in cc_compiler and "g++" not in cxx_compiler)
        or ("icx" in cc_compiler and "icpx" not in cxx_compiler)
        or ("icc" in cc_compiler and "icpc" not in cxx_compiler)
    ):
        exit_with_error(
            f"Compilers are not compatible: {cxx_compiler=}, {cc_compiler=}"
        )

    cxx_version = get_program_version(cxx_compiler)
    cc_version = get_program_version(cc_compiler)

    if cxx_version is None or cc_version is None:
        exit_with_error("Couldn't get the version of the compilers")
    elif cxx_version != cc_version:
        exit_with_error(
            f"Compiler versions are not compatible: {cxx_version=}, {cc_version=}"
        )

def supports_cmake_linker_type():
    cmake_version_str = get_program_version("cmake")
    if not cmake_version_str:
        exit_with_error("CMake is not installed or its version could not be determined.")
    return version.parse(cmake_version_str) >= version.parse("3.29.0")

def should_add_linker_flags(linker):
    if linker == "default":
        return False
    if shutil.which(linker) is not None:
        return True
    exit_with_error(
        f"The specified linker '{linker}' was not found. "
        "Try installing it or switching to another."
    )