#!/bin/python3
# Gathers data from the binary stencils used for embeding and patching

import click
import subprocess
import json
import typing
import bisect

import _stencils
import _schema

from llvm_tools import llvm_tools_version_options


def run_llvm(tool: str, args, echo=False) -> str:
    if echo:
        print(tool, *args, sep=" ")
    result = subprocess.run([tool] + args, check=True, capture_output=True, text=True)
    return result.stdout


def parse_relocation(
    relocation: _schema.ELFRelocation, stencil: _stencils.Stencil
) -> _stencils.Hole:
    return _stencils.Hole(
        offset=relocation["Offset"] - stencil.place,
        addend=relocation["Addend"],
        value=_stencils.symbol_to_value(relocation["Symbol"]["Name"]),
        kind=relocation["Type"]["Name"],
        symbol=relocation["Symbol"],
    )


def parse_stencil_section(stencil_section: _schema.ELFSection) -> _stencils.Stencil:
    output = _stencils.Stencil(
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


def is_stencil_section(section: _schema.ELFSection) -> bool:
    name = section["Name"]["Name"]
    if not name.startswith(".ltext."):
        return False

    global all_sections
    return True if all_sections else "stencil" in name


def get_stencil_name(section: _schema.ELFSection) -> str:
    name = section["Name"]["Name"]
    return name[len(".ltext.") :]


def split_section_relocations(
    section: _schema.ELFSection, stencils: list[_stencils.Stencil]
):
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


def parse(
    llvm_readobj: str, binary: str, verbose: bool, shared: bool
) -> list[_stencils.Stencil]:
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
    sections: list[_schema.ELFSection] = [
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
    llvm_readobj: str, binary, output_file, verbose: bool, shared: bool
):
    stencil_holes = parse(llvm_readobj, binary, verbose, shared)
    output_file.write(_stencils.stencils_to_c(stencil_holes, binary=binary.read()))


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
@click.argument("binary", type=click.File("rb"))  # rb = read binary
@llvm_tools_version_options
def main(llvm_readobj, output, binary, verbose, accept_all_sections, shared, **kwargs):
    global all_sections
    all_sections = accept_all_sections
    generate_stencils(llvm_readobj, binary, output, verbose, shared)


if __name__ == "__main__":
    main()
