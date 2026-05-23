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


def parse_relocation(relocation: _schema.ELFRelocation) -> _stencils.Hole:
    return _stencils.Hole(
        offset=relocation["Offset"],
        addend=relocation["Addend"],
        value=_stencils.symbol_to_value(relocation["Symbol"]["Name"]),
        kind=relocation["Type"]["Name"],
        symbol=relocation["Symbol"],
    )


def parse_stencil_section(stencil_section: _schema.ELFSection) -> _stencils.Stencil:
    return _stencils.Stencil(
        name=stencil_section["Name"]["Name"],
        place=stencil_section["Offset"],
        size=stencil_section["Size"],
        holes=[
            parse_relocation(rel["Relocation"])
            for rel in stencil_section["Relocations"]
        ],
    )


def is_stencil_section(name: str) -> bool:
    global lenient_sections
    if lenient_sections:
        return True
    
    return ".rela.ltext" in name and "stencil" in name


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
    sections: list[dict[typing.Literal["Section"], _schema.ELFSection]] = file_info[
        "Sections"
    ]

    print(f"sections: {len(sections)}")
    return [
        parse_stencil_section(section["Section"])
        for section in sections
        if is_stencil_section(section["Section"]["Name"]["Name"])
    ]


def generate_stencils(llvm_readobj: str, binary: str, output_file, verbose: bool):
    stencil_holes = parse(llvm_readobj, binary, verbose)
    output_file.write(
        _stencils.stencils_to_c(
            stencil_holes, binary=bytes(binary, encoding="UTF-8")  # read file as bytes
        )
    )


@click.command()
@click.option("--lenient")
@click.option(
    "--output",
    required=True,
    type=click.File("w"),
    help="File where stencils will be written.",
)
@click.option("-v", "--verbose", is_flag=True)
@click.argument("binary", type=click.Path(exists=True))
@llvm_tools_version_options
def main(llvm_readobj, output, binary, verbose, lenient, **kwargs):
    global lenient_sections
    lenient_sections = lenient
    generate_stencils(llvm_readobj, binary, output, verbose)


if __name__ == "__main__":
    main()
