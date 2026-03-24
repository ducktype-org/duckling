"""

Author: Wojciech Rzepliński
"""

import enum
import dataclasses
import _schema

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

    def as_c(self) -> str:
        """Dump this hole as an initialization of a C Hole struct."""
        parts = [
            f"{self.offset:#x}",
            f"HoleKind::{self.kind}",
            f"HoleValueType::{self.value.name}",
            f"{_signed(self.addend):#x}",
        ]
        return f"{{{', '.join(parts)}}}"


@enum.unique
class SectionType(enum.Enum):
    CODE = enum.auto()
    DATA = enum.auto()

@dataclasses.dataclass
class Stencil:
    """
    A contiguous block of machine code or data to be copied-and-patched.

    Analogous to a section or segment in an object file.
    """

    body: bytearray = dataclasses.field(default_factory=bytearray, init=False)
    holes: list[Hole] = dataclasses.field(default_factory=list, init=False)

    def pad(self, alignment: int) -> None:
        """Pad the stencil to the given alignment."""
        offset = len(self.body)
        padding = -offset % alignment
        self.body.extend([0x00] * padding)

    def remove_jump(self) -> None:
        """Remove a zero-length continuation jump, if it exists."""
        if not self.holes:
            return
        hole = max(self.holes, key=lambda hole: hole.offset)
        match hole:
            case Hole(
                offset=offset,
                kind="R_X86_64_GOTPCRELX",
                value=HoleValue.GOT,
                symbol="_continue_fn",
                addend=addend,
            ) as hole:
                assert _signed(addend) == -4
                # jmp qword ptr [rip]
                jump = b"\xFF\x25\x00\x00\x00\x00"
                offset -= 2
            case Hole(
                offset=offset,
                kind="R_X86_64_PLT32",
                value=HoleValue.CONTINUE_FUNCTION,
                symbol="_continue_fn",
                addend=addend,
            ) as hole:
                assert _signed(addend) == -4
                # jmp qword ptr [rip]
                jump = b"\xE9\x00\x00\x00\x00"
                offset -= 1
            case _:
                return
        if self.body[offset:] == jump:
            self.body = self.body[:offset]
            self.holes.remove(hole)

@dataclasses.dataclass
class StencilGroup:
    code: Stencil = dataclasses.field(default_factory=Stencil, init=False)
    data: Stencil = dataclasses.field(default_factory=Stencil, init=False)
    sections_info: dict[int, HoleValue] = dataclasses.field(default_factory=dict, init=False)

    _got: dict[str, int] = dataclasses.field(default_factory=dict, init=False)

    def process_relocations(self, *, alignment: int = 1) -> None:
        """Fix up all GOT and internal relocations for this stencil group."""

        self.code.remove_jump()
        # self.code.pad(16)
        # self.data.pad(8)

        for stencil in [self.code, self.data]:
            for hole in stencil.holes:
                if hole.value is HoleValue.GOT:
                    assert hole.symbol is not None
                    hole.value = HoleValue.DATA_ENTRY
                    hole.addend += self._global_offset_table_lookup(hole.symbol)
                    hole.symbol = None
        
        self._emit_global_offset_table()
        self.code.holes.sort(key=lambda hole: hole.offset)
        self.data.holes.sort(key=lambda hole: hole.offset)

    def _global_offset_table_lookup(self, symbol: str) -> int:
        # check if the symbol is already in the GOT
        if symbol not in self._got:
            self._got[symbol] = 8 * len(self._got)
        
        return len(self.data.body) + self._got[symbol]

    def _emit_global_offset_table(self) -> None:
        got = len(self.data.body)
        for s, offset in self._got.items():
            value, symbol = symbol_to_value(s), s
            addend = 0

            self.data.holes.append(
                Hole(got + offset, "R_X86_64_64", value, symbol, addend)
            )
            self.data.body.extend([0] * 8)


def _handle_section(
    section: _schema.ELFSection, group: StencilGroup
) -> None:

    # SH_RELA - relocation section
    # SH_PROGBITS - data or code section
    section_type = section["Type"]["Value"]
    flags = {flag["Name"] for flag in section["Flags"]["Flags"]}

    # Relocations section.
    if section_type == "SHT_RELA":

        # In the "Info" field in the SHT_RELA section, we store 
        # index of the section to which the relocation applies.
        # https://refspecs.linuxbase.org/elf/gabi4+/ch4.sheader.html
        value = group.sections_info[section["Info"]]
        if value is SectionType.CODE:
            stencil = group.code
        else:
            stencil = group.data
        
        for wrapped_relocation in section["Relocations"]:
            relocation = wrapped_relocation["Relocation"]
            hole = _handle_relocation(relocation, stencil.body)
            stencil.holes.append(hole)
        
    elif section_type == "SHT_PROGBITS":
        if "SHF_ALLOC" not in flags:
                return
        
        # Flag SHF_EXECINSTR indicates code, otherwise data.
        if "SHF_EXECINSTR" in flags:
            section_type = SectionType.CODE
            stencil = group.code
        else:
            section_type = SectionType.DATA
            stencil = group.data
        
        # Save the (code/data) info of the section for future
        group.sections_info[section["Index"]] = section_type
            
        stencil.body.extend(section["SectionData"]["Bytes"])
        assert not section["Relocations"]
    else:
        assert section_type in {
            "SHT_GROUP",
            "SHT_LLVM_ADDRSIG",
            "SHT_NULL",
            "SHT_STRTAB",
            "SHT_SYMTAB",
            "SHT_LLVM_LINKER_OPTIONS"
        }, section_type

def _handle_relocation(
    relocation: _schema.ELFRelocation, raw: bytes
) -> Hole:
    symbol: str | None
    match relocation:
        case {
            "Addend": addend,
            "Offset": offset,
            "Symbol": {"Value": s},
            "Type": {
                "Value": "R_AARCH64_ADR_GOT_PAGE"
                | "R_AARCH64_LD64_GOT_LO12_NC"
                | "R_X86_64_GOTPCREL"
                | "R_X86_64_GOTPCRELX"
                | "R_X86_64_REX_GOTPCRELX" as kind
            },
        }:
            value, symbol = HoleValue.GOT, s
        case {
            "Addend": addend,
            "Offset": offset,
            "Symbol": {"Value": s},
            "Type": {"Value": kind},
        }:
            value, symbol = symbol_to_value(s), s
        case _:
            raise NotImplementedError(relocation)
    return Hole(offset, kind, value, symbol, addend)

def _signed(value: int) -> int:
    value %= 1 << 64
    if value & (1 << 63):
        value -= 1 << 64
    return value
