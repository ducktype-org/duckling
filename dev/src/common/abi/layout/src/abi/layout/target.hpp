/**
 * @file target.hpp
 *
 * @brief Parameters required to compute a C-compatible layout for a given
 * target platform: an architecture identifier, endianness and the primitive
 * size/alignment information that the layout algorithm consults.
 *
 * The library ships ready-made presets for the two targets covered in this
 * PR (x86_64-linux and aarch64-linux). Callers are free to construct a
 * `TargetABI` by hand if they need something different.
 */
#pragma once

#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <cstdint>

namespace abi::layout {

	// NOLINTNEXTLINE(readability-identifier-naming) — "X86_64" is the canonical arch name.
	enum class Arch : uint8_t { X86_64, AArch64 };

	enum class Endianness : uint8_t { Little, Big };

	/**
	 * @brief Identifies a target architecture. OS and vendor do not affect C
	 * layout for the targets covered in this PR, so they are intentionally
	 * absent.
	 */
	struct TargetTriple final {
		Arch arch;
	};

	/**
	 * @brief Primitive size and alignment information that the C layout
	 * algorithm needs. All values are expressed in bytes.
	 */
	struct DataLayout final {
		Endianness endianness;
		Bytes      pointer_size;
		Bytes      pointer_alignment;

		/**
		 * @brief Returns the natural alignment in bytes for an integer of the
		 * given bit width. For the integer widths in scope (8, 16, 32, 64)
		 * natural alignment equals the size on both supported targets.
		 */
		[[nodiscard]]
		Bytes naturalAlignmentForIntWidth(u8 width_bits) const;
	};

	/**
	 * @brief Pair of (triple, data layout) carried together as a single value
	 * because every layout computation needs both.
	 */
	struct TargetABI final {
		TargetTriple triple;
		DataLayout   data_layout;
	};

	/** @brief Preset describing the x86_64-linux System V AMD64 ABI. */
	// NOLINTNEXTLINE(readability-identifier-naming) — "x86_64" is the canonical arch name.
	TargetABI x86_64Linux();

	/** @brief Preset describing the aarch64-linux AAPCS64 ABI. */
	TargetABI aarch64Linux();

}
