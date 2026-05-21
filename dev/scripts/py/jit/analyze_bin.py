#!/bin/python3
# Gathers data from the binary stencils used for embeding and patching

import click
import subprocess
import json
import typing
import _stencils
import _schema

from llvm_tools import llvm_tools_version_options

def run_llvm(tool: str, args, echo=False) -> str:
    if echo:
        print(tool, *args, sep=" ")
    result = subprocess.run([tool] + args, check=True, capture_output=True, text=True)
    return result.stdout

def process_stencil(stencil: _stencils.Stencil) -> _stencils.Stencil:
    pass

def parse_relocation(relocation: _schema.ELFRelocation) -> _stencils.Hole:
    output = _stencils.Hole()
    output.offset = relocation.Offset
    output.addend = relocation.Addend
    output.value = _stencils.symbol_to_value(relocation.symbol["Value"])
    output.kind = relocation.Type["Value"]
    return output

def parse_stencil_section(stencil_section: _schema.ELFSection) -> _stencils.Stencil:
    output = _stencils.Stencil()
    print(type(stencil_section))
    output.body = stencil_section.SectionData["Bytes"]
    output.holes = [parse_relocation(rel["Relocation"]) for rel in stencil_section.relocations]
    return output

def parse(llvm_readobj: str, binary: str, verbose: bool):
    readobj_args = [
        "--elf-output-style=JSON",
        "--expand-relocs",
        "--pretty-print",
        "--section-data",
        "--section-relocations",
        "--section-symbols",
        "--sections",
        f"{binary}",
    ]
    readobj_output = run_llvm(llvm_readobj, readobj_args, echo=verbose)

    file_info = json.loads(readobj_output)[0]
    sections: list[dict[typing.Literal["Section"], _schema.ELFSection]] = file_info["Sections"]

    print(f"sections: {len(sections)}")
    return [
        parse_stencil_section(section["Section"])
        for section in sections
        if section["Section"]["Name"]["Name"].startswith(".text.")
    ]


def generate_stencils(llvm_readobj: str, binary: str, output_file, verbose: bool):
    for stencil_section in parse(llvm_readobj, binary, verbose):
        output_file.write(process_stencil(stencil_section).to_c())


@click.command()
@click.option(
    "--output",
    required=True,
    type=click.File("w"),
    help="File where stencils will be written.",
)
@click.option("-v", "--verbose", is_flag=True)
@click.argument("binary", type=click.Path(exists=True))
@llvm_tools_version_options
def main(llvm_readobj, output, binary, verbose, **kwargs):
    generate_stencils(llvm_readobj, binary, output, verbose)


if __name__ == "__main__":
    main()
