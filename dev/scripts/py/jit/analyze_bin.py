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


def parse_stencil_section(stencil_section: ELFSection) -> Stencil:
    output = Stencil(
        name=get_stencil_name(stencil_section),
        place=stencil_section["Offset"],
        type=StencilType.INSTRUCTION,
        size=stencil_section["Size"],
        holes=[],
    )
    if "special" in output.name:
        output.type = StencilType.SPECIAL
    output.holes = [
        parse_relocation(rel["Relocation"], output)
        for rel in stencil_section["Relocations"]
    ]
    return output


def is_stencil_section(section: ELFSection) -> bool:
    name = section["Name"]["Name"]
    if not name.startswith(".ltext."):
        return False

    global all_sections
    return True if all_sections else "stencil" in name


def get_stencil_name(section: ELFSection) -> str:
    name = section["Name"]["Name"]
    return name[len(".ltext.") :]


def split_section_relocations(section: ELFSection, stencils: list[Stencil]) -> list[Stencil]:
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

def split_shared_relocations(sections: list[ELFSection], stencils: list[Stencil]) -> list[Stencil]:
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
    return [
        section["Section"] for section in file_info["Sections"]
    ]

def order_stencils(stencils: list[Stencil], order) -> list[Stencil]:
    no_stencil = Stencil(name="NO STENCIL", type=StencilType.NO_STENCIL, place=0, size=0, holes=[])
    array = [no_stencil] * len(order)
    for stencil in stencils:
        if stencil.name.startswith("stencil_special"):
            continue
        idx = order[stencil.name[len("stencil_") :]]
        array[idx] = stencil

    for stencil in stencils:
        if stencil.name.startswith("stencil_special"):
            array.append(stencil)

    return array

def generate_stencils(
    llvm_readobj: str, binary, verbose: bool, shared: bool, order
) -> list[Stencil]:
    sections = parse(llvm_readobj, binary, verbose)
    
    stencils = [
        parse_stencil_section(section)
        for section in sections
        if is_stencil_section(section)
    ]
    if shared:
        stencils = split_shared_relocations(sections, stencils)
    if order:
        stencils = order_stencils(stencils, json.loads(order.read()))

    for stencil in stencils:
        if not stencil.validate():
            print(f"Stencil: {stencil} failed validation")
            exit(4)
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
@click.option("--order", type=click.File("r"))
@click.argument("binary", type=click.File("rb"))  # rb = read binary
@llvm_tools_version_options
def main(
    llvm_readobj, output, binary, verbose, accept_all_sections, shared, order, **kwargs
):
    global all_sections
    all_sections = accept_all_sections
    
    stencils = generate_stencils(llvm_readobj, binary, verbose, shared, order)

    output.write(stencils_to_c(stencils, binary=binary.read()))


if __name__ == "__main__":
    main()
