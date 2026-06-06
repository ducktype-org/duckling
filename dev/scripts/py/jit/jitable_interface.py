"""
Removes functions unnecessary for jit. Input file should be output of llvm-nm.
"""
import click

from llvm_tools import llvm_tools_version_options, run_llvm_tool

def is_opfun(func_name: str) -> bool:
    return func_name.startswith("vm::OpFuns::op_") and not func_name.startswith(
        "vm::OpFuns::op_debug"
    )

def is_stencil(func_name: str) -> bool:
    return func_name.startswith("stencil")

def nonjitable(func_name: str) -> bool:
    unjitable_opfuncs = [
        "jit_call_entrypoint",
        "call_func",
        "call_builtinfunc",
        "virtual_call_pptr_method",
        "ret_tailcall_func",
        "step_gil",
        "check_strategy",
        "breakpoint",
    ]

    return any(op in func_name for op in unjitable_opfuncs)

special_functions = {}
def is_special_function(func_name: str) -> bool:
    global special_functions

    for special_function in special_functions:
        if special_function in func_name:
            special_functions[special_function] = True
            return True
    else:
        return False


def should_remain(func_name: str) -> bool:
    return is_special_function(func_name) or (is_opfun(func_name) or is_stencil(func_name)) and not nonjitable(func_name)


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
@llvm_tools_version_options
def main(llvm_nm, llvm_cxxfilt, input_path, output_file, **kwargs):
    write = lambda what: output_file.write(what + "\n")

    nm_flags = ["--portability", input_path]
    functions = run_llvm_tool(llvm_nm, nm_flags)

    mangled_names = [
        " ".join(name_split)
        for line in functions.splitlines()
        for *name_split, _type, _place, _size in [line.split(" ")]
    ]

    unmangled_names = run_llvm_tool(llvm_cxxfilt, args=[], input='\n'.join(mangled_names)).splitlines()

    for mangled, unmangled in zip(mangled_names, unmangled_names):
        if should_remain(unmangled):
            write(mangled)

    global special_functions
    for func_name, used in special_functions.items():
        if not used:
            print(f"Function: '{func_name}' not found")


if __name__ == "__main__":
    main()
