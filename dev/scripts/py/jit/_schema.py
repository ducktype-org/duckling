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
    Symbol: dict[typing.Literal["Name"], str]
    Type: dict[typing.Literal["Name"], HoleKind]

class ELFSymbol(typing.TypedDict):
    Name: dict[typing.Literal["Name"], str]
    Value: int


class ELFSection(typing.TypedDict):
    """An ELF object file section."""

    Index: int
    Name: ELFSymbol
    Type: dict[typing.Literal["Value"], str]
    Flags: dict[typing.Literal["Flags"], list[dict[typing.Literal["Name"], str]]]
    Offset: int
    Size: int
    Info: int
    Relocations: list[dict[typing.Literal["Relocation"], ELFRelocation]]
    Symbols: list[dict[typing.Literal["Symbol"], ELFSymbol]]
    SectionData: dict[typing.Literal["Bytes"], list[int]]
