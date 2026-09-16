"""
Defines stencils, and how they are printed to c++

Authors: Wojciech Rzepliński, Paweł Mieszkowski
"""

import enum
import dataclasses


def list_quote(elements):
    return "{" + ", ".join(elements) + "}"

@enum.unique
class HoleValue(enum.Enum):
    INSTR_PTR = enum.auto()
    ARG0 = enum.auto()
    ARG1 = enum.auto()
    
    CONTINUE_FN = enum.auto()
    JMP_FN = enum.auto()
    CALL_FN = enum.auto()

    CALL_OPCODE = enum.auto()

    NONE = enum.auto()


def symbol_to_value(symbol: str) -> HoleValue:
    if not symbol.startswith("_value_to_patch_"):
        return HoleValue.NONE
    
    type_name = symbol[len("_value_to_patch_"):]
    return HoleValue[type_name.upper()]

def to_pascal_case(text):
    return "".join(word.capitalize() for word in text.split("_"))

@dataclasses.dataclass
class Hole:
    offset: int
    value: HoleValue

    def to_c(self) -> str:
        return "StencilHole" + list_quote(
            [
                f".offset = {self.offset}",
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
    binary_name: str
    unmangled_name: str
    type: StencilType
    offset: int
    size: int
    holes: list[Hole]

    def stencil_binary(self, binary):
        return binary[self.offset: self.offset + self.size]

    def remove_jump(self, binary):
        # This only checks `jmp rax`, but that is enough as most stencils end in exactly this way.
        # This is very dependant on the compilation method used, and I have my doubts that it is the best way, so I leave it be for now.
        if  self.type == StencilType.INSTRUCTION and self.stencil_binary(binary).endswith(b"\xFF\xE0"):
            self.size = self.size - 2
            return True
        return False

    def validate(self) -> bool:
        match self.type:
            case StencilType.NO_STENCIL:
                return True
            case StencilType.SPECIAL:
                return self.size != 0
            case StencilType.INSTRUCTION:
                return self.size != 0 and len(self.holes) != 0

    def to_c(self) -> str:
        return "StencilData " + list_quote(
            [
                f'.name = "{self.binary_name}"',
                f".place = {self.offset}",
                f".size = {self.size}",
                ".to_patch = "
                + list_quote(
                    hole.to_c() for hole in self.holes if hole.value != HoleValue.NONE
                ),
            ]
        )


def stencils_to_c(stencils) -> str:
    return f"std::array<StencilData, {len(stencils)}>" + list_quote(
        [stencil.to_c() for stencil in stencils]
    )
