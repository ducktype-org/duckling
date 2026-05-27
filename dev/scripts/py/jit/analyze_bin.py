#!/bin/python3
# Gathers data from the binary stencils used for embeding and patching

import click
import subprocess
import json
import typing
import bisect

from _stencils import Stencil, Hole, symbol_to_value, stencils_to_c
from _schema import ELFRelocation, ELFSection

from llvm_tools import llvm_tools_version_options


def run_llvm(tool: str, args, echo=False) -> str:
    if echo:
        print(tool, *args, sep=" ")
    result = subprocess.run([tool] + args, check=True, capture_output=True, text=True)
    return result.stdout


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
        size=stencil_section["Size"],
        holes=[],
    )
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


def split_section_relocations(section: ELFSection, stencils: list[Stencil]):
    stencil_offset = lambda stencil: stencil.place
    beginnings = sorted([stencil for stencil in stencils], key=stencil_offset)
    for wrapped_relocation in section["Relocations"]:
        relocation = wrapped_relocation["Relocation"]
        rel_offset = relocation["Offset"]

        idx = bisect.bisect_right(beginnings, rel_offset, key=stencil_offset)
        if idx == 0:
            print(f"Ignored: {rel_offset}")
            continue

        print(f"Found: {idx - 1}")
        stencil = stencils[idx - 1]
        if rel_offset < stencil.place + stencil.size:
            stencil.holes.append(parse_relocation(relocation, stencil))


def parse(llvm_readobj: str, binary: str, verbose: bool, shared: bool) -> list[Stencil]:
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
    readobj_output = run_llvm(llvm_readobj, readobj_args, echo=verbose)

    file_info = json.loads(readobj_output)[0]
    sections: list[ELFSection] = [
        section["Section"] for section in file_info["Sections"]
    ]

    print(f"sections: {len(sections)}")
    stencils = [
        parse_stencil_section(section)
        for section in sections
        if is_stencil_section(section)
    ]

    if shared:
        rel = next(
            (section for section in sections if section["Name"]["Name"] == ".rela.dyn"),
            None,
        )
        if not rel:
            print("No relocations section in a shared object")
            exit(2)
        split_section_relocations(rel, stencils)
        for stencil in stencils:
            print(len(stencil.holes))
        return stencils


def generate_stencils(
    llvm_readobj: str, binary, output_file, verbose: bool, shared: bool, order
):
    stencils = parse(llvm_readobj, binary, verbose, shared)
    if order:
        order_map = json.loads(order.read())
        no_stencil = Stencil(name="NO STENCIL", place=0, size=0, holes=[])
        array = [no_stencil] * len(order_map)
        for stencil in stencils:
            idx = order_map[stencil.name[len("stencil_") :]]
            array[idx] = stencil

        stencils = array
        for stencil in stencils:
            opcode = order_map[stencil.name[len("stencil_") :]] if stencil.name != no_stencil.name else -1
            print(f"name: {stencil.name}, opcode: {opcode}")
    output_file.write(stencils_to_c(stencils, binary=binary.read()))


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
    generate_stencils(llvm_readobj, binary, output, verbose, shared, order)


if __name__ == "__main__":
    main()
