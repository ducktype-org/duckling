import pathlib
import subprocess as sp
import sys

import click


def with_venv(cmd):
    if not pathlib.Path(".venv").exists():
        exit_with_error('.venv does not exits. Use "./toolbox.py setup-venv"')

    bash_command(f"source .venv/bin/activate && {cmd}")


def exit_with_error(msg):
    click.echo(click.style("[ERROR]: ", fg="red", bold=True), nl=False)
    click.echo(click.style(msg, fg="red"))

    exit(1)


class BashCommandError(Exception):
    def __init__(self, command: str, exit_code, stdout, stderr):
        super().__init__(
            f"\n\tBash command `{command}` has failed with a an exit code: {exit_code}, because:\n"
            + f"[STDOUT]:{"\n" + stdout if stdout else ""}\n"
            + f"[STDERR]:{"\n" + stderr if stderr else ""}"
        )
        self.command = command
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr


def bash_command(cmd, cwd=".", redirect=None, click_file=sys.stdout):
    log_bash(cmd, file=click_file)
    proc = sp.Popen(["/bin/bash", "-c", cmd], cwd=cwd, stdout=redirect, stderr=redirect)
    stdout, stderr = proc.communicate()

    if stdout is not None:
        stdout = stdout.decode("UTF-8")
    if stderr is not None:
        stderr = stderr.decode("UTF-8")

    status = proc.wait()
    if status != 0:
        raise BashCommandError(cmd, status, stdout, stderr)
    return stdout, stderr


def bash_command_get_output(cmd, cwd=".", click_file=sys.stdout):
    return bash_command(cmd, cwd, redirect=sp.PIPE, click_file=click_file)


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


def make_pretty_command(command):
    pretty_command = command.replace("\n", " ")
    while "  " in pretty_command:
        pretty_command = pretty_command.replace("  ", " ")
    return pretty_command


# this class overrides the click.Option class, so it can get ctx
# and infer and set the default value from other options
def default_compiler_from_ctx(default_name):

    class OptionDefaultFromCtx(click.Option):

        def get_default(self, ctx, call = True):
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
        elif "icc" in cc_compiler:
            cxx_compiler = cc_compiler.replace("icc", "icpc")
        else:
            cxx_compiler = "g++"
    else:
        cxx_compiler = "g++"

    return cxx_compiler
