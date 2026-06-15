"""
Defines stencils, and how they are printed to c++

Authors: Wojciech Rzepliński, Paweł Mieszkowski
"""

import enum
import dataclasses
import _schema


def list_quote(elements):
    return "{" + ", ".join(elements) + "}"

def _signed(value: int) -> int:
    value %= 1 << 64
    if value & (1 << 63):
        value -= 1 << 64
    return value


@enum.unique
class HoleValue(enum.Enum):
    """
    Different "base" values that can be patched into holes.
    """

    CONTINUE_FN = enum.auto()
    JMP_FN = enum.auto()
    CALL_FN = enum.auto()
    ARG0 = enum.auto()
    ARG1 = enum.auto()
    ZERO = enum.auto()


def symbol_to_value(symbol: str) -> HoleValue:
    """
    Convert a symbol name to a HoleValue and a symbol name.
    """
    if not symbol.startswith("_value_to_patch_"):
        return HoleValue.ZERO
    
    type_name = symbol[len("_value_to_patch_"):]
    return HoleValue[type_name.upper()]

def to_pascal_case(text):
    return "".join(word.capitalize() for word in text.split("_"))

@dataclasses.dataclass
class Hole:
    """
    A "hole" in the stencil to be patched with a computed runtime value.
    Analogous to relocation records in an object file.
    """

    offset: int
    kind: _schema.HoleKind
    # Patch with this base value:
    value: HoleValue
    # ...plus the address of this symbol:
    symbol: str | None
    # ...plus this addend:
    addend: int

    def to_c(self) -> str:
        return "StencilHole" + list_quote(
            [
                f".offset = {self.offset}",
                f".size = 4",
                f".type = HoleType::Movable",
                f".value = HoleValue::{to_pascal_case(self.value.name)}",
            ]
        )

@enum.unique
class StencilType(enum.Enum):
    NO_STENCIL = enum.auto()
    SPECIAL = enum.auto()
    INSTRUCTION = enum.auto()


@dataclasses.dataclass
class Stencil:
    """
    A contiguous block of machine code or data to be copied-and-patched.
    Analogous to a section or segment in an object file.
    """

    name: str
    type: StencilType
    place: int
    size: int
    holes: list[Hole]

    def stencil_binary(self, binary):
        return binary[self.place: self.place + self.size]

    def remove_jump(self, binary):
        # This only checks `jmp rax`, but that is enough as most stencils end in exaclty this way.
        # This is very dependant on the compilation method used, and I have my doubts that it is the best way, so I leave it be for now.
        if  self.type == StencilType.INSTRUCTION and self.stencil_binary(binary).endswith((b"\xFF", b"\xE0")):
            self.size = self.size - 2

    def validate(self) -> bool:
        match self.type:
            case StencilType.NO_STENCIL:
                return True
            case StencilType.SPECIAL:
                return self.size != 0
            case StencilType.INSTRUCTION:
                return self.size != 0 and len(self.holes)

    def to_c(self) -> str:
        return "StencilData " + list_quote(
            [
                f'.name = "{self.name}"',
                f".place = {self.place}",
                f".size = {self.size}",
                ".to_patch = "
                + list_quote(
                    hole.to_c() for hole in self.holes if hole.value != HoleValue.ZERO
                ),
                ".relocation = {}",
            ]
        )


def stencils_to_c(stencils) -> str:
    return f"std::array<StencilData, {len(stencils)}>" + list_quote(
        [stencil.to_c() for stencil in stencils]
    )
