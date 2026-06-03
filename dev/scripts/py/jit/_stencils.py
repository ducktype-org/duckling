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

def byte_to_c(byte) -> str:
    return "std::byte{0x" + f"{byte:02x}" + "}"


@enum.unique
class HoleValue(enum.Enum):
    """
    Different "base" values that can be patched into holes (usually combined with the
    address of a symbol and/or an addend).
    """

    # The base address of the machine code for the current uop (exposed as _jit_entry):
    CODE_ENTRY = enum.auto()
    # The base address of the read-only data for this uop:
    DATA_ENTRY = enum.auto()
    # The base address of the machine code for the next uop (exposed as continue_fn):
    CONTINUE_FUNCTION = enum.auto()

    JMP_FUNCTION = enum.auto()

    # Program counter, instruction number in the current function
    PC = enum.auto()

    ARG0 = enum.auto()

    ARG1 = enum.auto()
    # The base address of the "global" offset table located in the read-only data.
    # Shouldn't be present in the final stencils, since these are all replaced with
    # equivalent DATA values:
    GOT = enum.auto()
    # A hardcoded value of zero (used for symbol lookups):
    ZERO = enum.auto()


def symbol_to_value(symbol: str) -> HoleValue:
    """
    Convert a symbol name to a HoleValue and a symbol name.
    """
    if symbol == "_entry":
        return HoleValue.CODE_ENTRY
    if symbol == "_continue_fn":
        return HoleValue.CONTINUE_FUNCTION
    if symbol == "_arg0":
        return HoleValue.ARG0
    if symbol == "_arg1":
        return HoleValue.ARG1
    if symbol == "_jmp_fn":
        return HoleValue.JMP_FUNCTION
    if symbol == "_pc":
        return HoleValue.PC
    else:
        return HoleValue.ZERO


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
                f".value = HoleValue::{self.value.name}"
            ]
        )


@dataclasses.dataclass
class Stencil:
    """
    A contiguous block of machine code or data to be copied-and-patched.
    Analogous to a section or segment in an object file.
    """

    name: str
    place: int
    size: int
    holes: list[Hole]

    def validate(self) -> bool:
        return self.size != 0 and len(self.holes) 

    def to_c(self) -> str:
        return "StencilData " + list_quote(
            [
                f'.name = "{self.name}"',
                f".place = {self.place}",
                f".size = {self.size}",
                ".to_patch = " + list_quote(hole.to_c() for hole in self.holes if hole.value != HoleValue.ZERO),
                ".relocation = {}",
            ]
        )

def stencils_to_c(stencils, binary) -> str:
    return ', '.join(
        [
            f".stencils_binary = std::array<std::byte, {len(binary)}>"
            + list_quote([byte_to_c(byte) for byte in binary]),
            ".stencils_data = std::to_array<StencilData>("
            + list_quote([stencil.to_c() for stencil in stencils])
            + ")",
        ]
    )
