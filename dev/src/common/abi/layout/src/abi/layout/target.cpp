#include <abi/layout/target.hpp>

#include <base/except/exceptions.hpp>

namespace abi::layout {

	Bytes DataLayout::naturalAlignmentForIntWidth(u8 width_bits) const {
		switch (usize(width_bits)) {
		case 8:
			return Bytes(1);
		case 16:
			return Bytes(2);
		case 32:
			return Bytes(4);
		case 64:
			return Bytes(8);
		default:
			CORE_PANIC("unsupported integer width for C ABI: ", std::to_string(usize(width_bits)));
		}
	}

	// NOLINTNEXTLINE(readability-identifier-naming) — "x86_64" is the canonical arch name.
	TargetABI x86_64Linux() {
		return TargetABI{
			.triple      = TargetTriple{ .arch = Arch::X86_64 },
			.data_layout = DataLayout{
				.endianness        = Endianness::Little,
				.pointer_size      = Bytes(8),
				.pointer_alignment = Bytes(8),
			},
		};
	}

	TargetABI aarch64Linux() {
		return TargetABI{
			.triple      = TargetTriple{ .arch = Arch::AArch64 },
			.data_layout = DataLayout{
				.endianness        = Endianness::Little,
				.pointer_size      = Bytes(8),
				.pointer_alignment = Bytes(8),
			},
		};
	}

}
