#!/bin/python3

"""
Gathers data from the binary stencils used for embeding and patching.
"""

import click
import json
import bisect

from _stencils import Stencil, Hole, symbol_to_value, stencils_to_c, StencilType
from _schema import ELFRelocation, ELFSection

from llvm_tools import llvm_tools_version_options, run_llvm_tool

def parse_relocation(relocation: ELFRelocation, stencil: Stencil) -> Hole:
    return Hole(
        offset=relocation["Offset"] - stencil.place,
        addend=relocation["Addend"],
        value=symbol_to_value(relocation["Symbol"]["Name"]),
        kind=relocation["Type"]["Name"],
        symbol=relocation["Symbol"],
    )

def is_function_section(section: ELFSection):
    return section["Name"]["Name"].startswith(".ltext.")

def get_function_name(section: ELFSection):
    assert is_function_section(section)
    return section["Name"]["Name"][len(".ltext."):]

def parse_stencil_section(stencil_section: ELFSection, unmangled_name: str) -> Stencil:
    output = Stencil(
        unmangled_name=get_stencil_name(unmangled_name),
        binary_name=get_function_name(stencil_section),
        place=stencil_section["Offset"],
        type=StencilType.INSTRUCTION,
        size=stencil_section["Size"],
        holes=[],
    )
    if output.unmangled_name.startswith("special"):
        output.type = StencilType.SPECIAL
    output.holes = [
        parse_relocation(rel["Relocation"], output)
        for rel in stencil_section["Relocations"]
    ]
    return output


def is_stencil_section(unmangled_name: str) -> bool:
    return "stencil" in unmangled_name


def get_stencil_name(unmangled_name: str) -> str:
    return unmangled_name[len("vm::jit::cnp::stencil_"):-len("(vm::MicroInstruction const*, std::byte*, vm::Frame*, vm::SafeVMThread&)")]


def split_section_relocations(
    section: ELFSection, stencils: list[Stencil]
) -> list[Stencil]:
    stencil_offset = lambda stencil: stencil.place
    beginnings = sorted([stencil for stencil in stencils], key=stencil_offset)
    for wrapped_relocation in section["Relocations"]:
        relocation = wrapped_relocation["Relocation"]
        rel_offset = relocation["Offset"]

        idx = bisect.bisect_right(beginnings, rel_offset, key=stencil_offset)
        if idx == 0:
            continue
        stencil = stencils[idx - 1]
        if rel_offset < stencil.place + stencil.size:
            stencil.holes.append(parse_relocation(relocation, stencil))
    return stencils


def split_shared_relocations(
    sections: list[ELFSection], stencils: list[Stencil]
) -> list[Stencil]:
    shared_section = next(
        (section for section in sections if section["Name"]["Name"] == ".rela.dyn"),
        None,
    )
    if not shared_section:
        print("No relocations section in a shared object")
        exit(2)
    return split_section_relocations(shared_section, stencils)

def parse(llvm_readobj: str, binary: str, verbose: bool) -> list[Stencil]:
    readobj_args = [
        "--elf-output-style=JSON",
        "--expand-relocs",
        "--pretty-print",
        "--section-data",
        "--section-relocations",
        "--section-symbols",
        "--sections",
        f"{binary.name}",
    ]
    readobj_output = run_llvm_tool(llvm_readobj, readobj_args, echo=verbose)

    file_info = json.loads(readobj_output)[0]
    return [section["Section"] for section in file_info["Sections"]]


def order_stencils(stencils: list[Stencil], order) -> list[Stencil]:
    no_stencil = Stencil(
        unmangled_name="NO STENCIL", binary_name="NO STENCIL", type=StencilType.NO_STENCIL, place=0, size=0, holes=[]
    )
    array = [no_stencil] * len(order)
    for stencil in stencils:
        idx = order[stencil.unmangled_name]
        array[idx] = stencil
    return array


def validate_stencils(stencils: list[Stencil]):
    for stencil in stencils:
        if not stencil.validate():
            print(f"Stencil: {stencil} failed validation")
            exit(4)


def generate_stencils(
    llvm_readobj: str,
    llvm_cxxfilt: str,
    binary,
    accept_all_sections: bool,
    verbose: bool,
    shared: bool,
    order,
) -> list[Stencil]:
    sections = parse(llvm_readobj, binary, verbose)

    function_sections = [section for section in sections if is_function_section(section)]
    function_names = [get_function_name(section) for section in function_sections]
    unmangled_names = run_llvm_tool(llvm_cxxfilt, args=[], input="\n".join(function_names), echo=verbose).split('\n')

    stencils = [
        parse_stencil_section(section, unmangled_name)
        for section, unmangled_name in zip(function_sections, unmangled_names)
        if accept_all_sections or is_stencil_section(unmangled_name)
    ]
    assert len(stencils) != 0
    if shared:
        stencils = split_shared_relocations(sections, stencils)
    if order:
        stencils = order_stencils(stencils, json.loads(order.read()))
    if not accept_all_sections:
        validate_stencils(stencils)

    return stencils


@click.command()
@click.option("--accept-all-sections", is_flag=True)
@click.option(
    "--output",
    required=True,
    type=click.File("w"),
    help="File where stencils will be written.",
)
@click.option("-v", "--verbose", is_flag=True)
@click.option("-s", "--shared", is_flag=True)
@click.option("-r", "--remove-jumps", is_flag=True)
@click.option("--order", type=click.File("r"))
@click.argument("binary", type=click.File("rb"))
@llvm_tools_version_options
def main(
    llvm_readobj, llvm_cxxfilt, output, binary, verbose, accept_all_sections, shared, order, remove_jumps, **kwargs
):
    stencils = generate_stencils(
        llvm_readobj=llvm_readobj,
        llvm_cxxfilt=llvm_cxxfilt,
        binary=binary,
        verbose=verbose,
        accept_all_sections=accept_all_sections,
        shared=shared,
        order=order,
    )

    if remove_jumps:
        binary_contents = binary.read()
        print(type(binary_contents))
        for stencil in stencils:
            stencil.remove_jump(binary_contents)

    output.write(stencils_to_c(stencils))


if __name__ == "__main__":
    main()
