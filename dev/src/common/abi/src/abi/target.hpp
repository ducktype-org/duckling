#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <cstdint>

namespace abi {

	// NOLINTNEXTLINE(readability-identifier-naming) — "X86_64" is the canonical arch name.
	enum class Arch : uint8_t { X86_64, AArch64 };

	/**
	 * @brief The operating system of a target.
	 */
	enum class OperatingSystem : uint8_t { Linux, Darwin };

	enum class Endianness : uint8_t { Little, Big };

	/**
	 * @brief Identifies a target architecture and operating system.
	 */
	struct TargetTriple final {
		Arch            arch;
		OperatingSystem os;
	};

	/**
	 * @brief Stored size and alignment of a type, both expressed in bytes.
	 */
	struct SizeAlign final {
		Bytes size;
		Bytes alignment;
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
		 * @brief Per-target table mapping a floating-point width (in bits) to
		 * its stored size and alignment. A width that is absent from the table
		 * has no C representation on this target and must be rejected before
		 * layout.
		 *
		 * The IEEE binary formats (16/32/64/128) are present on every target;
		 * the x87 80-bit extended `long double` appears only on x87 targets,
		 * where its stored size (16) differs from `width / 8` — which is exactly
		 * why the value is tabulated per target instead of derived from width.
		 */
		base::Map<u64, SizeAlign> float_layouts;

		/**
		 * @brief Returns the natural alignment in bytes for an integer of the
		 * given bit width. Supported widths: 8, 16, 32, 64; for each, natural
		 * alignment equals the size.
		 */
		[[nodiscard]]
		static Bytes naturalAlignmentForIntWidth(u64 width_bits);
	};

	/**
	 * @brief Pair of (triple, data layout) carried together as a single value
	 * because every layout computation needs both.
	 */
	struct TargetABI final {
		TargetTriple triple{};
		DataLayout   data_layout;
	};

	/** @brief Preset describing the x86_64-linux System V AMD64 ABI. */
	// NOLINTNEXTLINE(readability-identifier-naming) — "x86_64" is the canonical arch name.
	const TargetABI& x86_64Linux();

	/** @brief Preset describing the aarch64-linux AAPCS64 ABI. */
	const TargetABI& aarch64Linux();

	/** @brief Preset describing the arm64-darwin ABI (AAPCS64 with Apple's deviations). */
	const TargetABI& aarch64Darwin();

	/**
	 * @brief The ABI of the architecture this binary was built for, selected at
	 * compile time from the toolchain's target macros.
	 */
	const TargetABI& hostTargetABI();

}
