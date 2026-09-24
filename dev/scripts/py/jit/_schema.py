"""
JSON schema from llvm-readobj.

Author: Wojciech Rzepliński, Paweł Mieszkowski
"""
import typing

class ELFName(typing.TypedDict):
    Name: str

class ELFRelocation(typing.TypedDict):
    Offset: int
    Symbol: ELFName

class ELFSection(typing.TypedDict):
    Name: ELFName
    Offset: int
    Size: int
    Relocations: list[dict[typing.Literal["Relocation"], ELFRelocation]]
