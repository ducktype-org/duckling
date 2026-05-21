"""

Schema for the JSON produced by llvm-readobj --elf-output-style=JSON.

Author: Wojciech Rzepliński
"""
import typing

HoleKind: typing.TypeAlias = typing.Literal[
    "R_X86_64_64",
    "R_X86_64_32",
    "R_X86_64_32S",
    "R_X86_64_GOTPCREL",
    "R_X86_64_GOTPCRELX",
    "R_X86_64_PC32",
    "R_X86_64_PLT32"
    "R_X86_64_REX_GOTPCRELX",
]


class ELFRelocation(typing.TypedDict):
    """An ELF object file relocation record."""

    Addend: int
    Offset: int
    Symbol: dict[typing.Literal["Value"], str]
    Type: dict[typing.Literal["Value"], HoleKind]

class ELFSymbol(typing.TypedDict):
    Name: dict[typing.Literal["Value"], str]
    Value: int


class ELFSection(typing.TypedDict):
    """An ELF object file section."""

    Index: int
    Name: ELFSymbol
    Flags: dict[typing.Literal["Flags"], list[dict[typing.Literal["Name"], str]]]
    Info: int
    Relocations: list[dict[typing.Literal["Relocation"], ELFRelocation]]
    SectionData: dict[typing.Literal["Bytes"], list[int]]
    Symbols: list[dict[typing.Literal["Symbol"], ELFSymbol]]
    Type: dict[typing.Literal["Value"], str]
    
