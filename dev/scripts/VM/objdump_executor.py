#!/usr/bin/env python3

import re
import subprocess
import pathlib
import click
import shlex


@click.command(no_args_is_help=True)
@click.argument("filepath", type=click.Path(exists=True))
@click.option("-function", help="Function to search for.")
@click.option("-o", help="Output file.", type=click.Path())
@click.option("-v", default=True, help="Verbose.")
def save_objdump(filepath, function, o, v):
    """Dumps `opFuns` and `internalCallMain` functions from VM to assembly.
    Calls "objdump" on VM executable binary file and saves the output to a text file."""
    basename = pathlib.Path(filepath).stem
    result_file_path = o or basename + ".asm"

    objdump = "objdump"
    args = [
        "-d",  # Disassemble the contents of the file
        "-C",  # Demangle C++ symbols
        "-S",  # Display source code intermixed with disassembly if possible. Does nothing if not
        "--no-show-raw-insn",  # Do not display raw instruction bytes
        "-M",
        "intel",  # Use intel syntax
    ]
    cmd = [objdump, *args, filepath]
    click.echo(shlex.join(cmd))

    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    executor_asm = filter_results(res.stdout.decode(), function)

    with open(result_file_path, "w") as f:
        f.write(executor_asm)

    click.echo("Output saved to " + result_file_path)


def prettify_function_body(lines):
    fun_name = lines[0]
    lines = lines[1:]

    # Removes line numbers from objdump output
    lines = [line.split(":")[1:] for line in lines[1:]]  # remove line numbers
    lines = [":".join(line) for line in lines]
    lines = ["   " + line.strip() for line in lines]  # indent lines

    return fun_name + "\n" + "\n".join(lines)


def filter_results(output: str, search_for_function):
    functions = output.split("\n\n")
    output = ""

    pattern = r"<(.+)\(.*\).*>"
    for function in functions:
        lines = function.split("\n")
        function_names = re.findall(pattern, lines[0])
        if len(function_names) == 0:
            continue
        function_name = function_names[0]
        if "OpFuns" in function_name:
            output += prettify_function_body(lines) + "\n\n"
        if "internalCall" in function_name:
            output = prettify_function_body(lines) + "\n\n" + output
        if search_for_function and search_for_function in function_name:
            return prettify_function_body(lines)

    return output


if __name__ == "__main__":
    save_objdump()
