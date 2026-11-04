# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

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
