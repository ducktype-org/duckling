# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Removes functions unnecessary for jit. Input file should be output of llvm-nm.
The list of non-jittable opcodes comes from C++ (non_jittable.def.hpp), exported by
print_opcodes inside the opcodes JSON passed via --opcodes.
"""
import click
import json

from llvm_tools import llvm_tools_version_options, run_llvm_tool

def is_opfun(func_name: str) -> bool:
    return func_name.startswith("vm::OpFuns::op_") and not func_name.startswith(
        "vm::OpFuns::op_debug"
    )

def is_stencil(func_name: str) -> bool:
    return func_name.startswith("vm::jit::cnp::stencil")

def nonjittable(func_name: str, nonjittable_opfuncs: list[str]) -> bool:
    # Exact match on the opcode name, not a substring check: one opcode name can contain another
    # (e.g. "retTailcall_func" contains "call_func"). `func_name` may be fully qualified
    # ("vm::OpFuns::op_call_func_off(...)", "vm::jit::cnp::stencil_call_func_off(...)") or bare
    # ("call_func_off", as in opcodes.json).
    short = func_name.split("(")[0].split("::")[-1]
    for prefix in ("op_", "stencil_"):
        short = short.removeprefix(prefix)
    return short in nonjittable_opfuncs

def should_remain(func_name: str, nonjittable_opfuncs: list[str]) -> bool:
    return (is_opfun(func_name) or is_stencil(func_name)) and not nonjittable(func_name, nonjittable_opfuncs)


@click.command()
@click.option(
    "-i",
    "--input",
    "input_path",
    type=click.Path(exists=True, readable=True),
    required=True,
)
@click.option(
    "-o",
    "--output",
    "output_file",
    type=click.File("w"),
    required=True,
)
@click.option(
    "--opcodes",
    "opcodes_path",
    type=click.Path(exists=True, readable=True),
    required=True,
    help="JSON opcode metadata from print_opcodes; its 'nonjittable' list is used for filtering.",
)
@llvm_tools_version_options
def main(llvm_nm, llvm_cxxfilt, input_path, output_file, opcodes_path, **kwargs):
    write = lambda what: output_file.write(what + "\n")

    with open(opcodes_path) as opcodes_file:
        nonjittable_opfuncs = json.load(opcodes_file)["nonjittable"]

    nm_flags = ["--portability", input_path]
    functions = run_llvm_tool(llvm_nm, nm_flags)

    mangled_names = [
        " ".join(name_split)
        for line in functions.splitlines()
        for *name_split, _type, _place, _size in [line.split(" ")]
    ]

    unmangled_names = run_llvm_tool(llvm_cxxfilt, args=[], input='\n'.join(mangled_names)).splitlines()

    for mangled, unmangled in zip(mangled_names, unmangled_names):
        if should_remain(unmangled, nonjittable_opfuncs):
            write(mangled)


if __name__ == "__main__":
    main()
