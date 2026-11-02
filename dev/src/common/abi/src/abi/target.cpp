// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <abi/target.hpp>

#include <base/config/target_info.hpp>
#include <base/except/exceptions.hpp>

#include <utility>

namespace abi {

	Bytes DataLayout::naturalAlignmentForIntWidth(u64 width_bits) {
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

	namespace {
		/**
		 * @brief The IEEE binary floating-point formats available on every
		 * supported target: width (bits) to stored size and alignment.
		 */
		base::Map<u64, SizeAlign> ieeeFloatLayouts() {
			base::Map<u64, SizeAlign> floats;
			floats.put(16, SizeAlign{ .size = Bytes(2), .alignment = Bytes(2) });
			floats.put(32, SizeAlign{ .size = Bytes(4), .alignment = Bytes(4) });
			floats.put(64, SizeAlign{ .size = Bytes(8), .alignment = Bytes(8) });
			floats.put(128, SizeAlign{ .size = Bytes(16), .alignment = Bytes(16) });
			return floats;
		}
	}

	// NOLINTNEXTLINE(readability-identifier-naming) — "x86_64" is the canonical arch name.
	const TargetABI& x86_64Linux() {
		static const TargetABI abi = [] {
			base::Map<u64, SizeAlign> floats = ieeeFloatLayouts();
			// x87 80-bit `long double`: 10 bytes of data padded to 16.
			floats.put(80, SizeAlign{ .size = Bytes(16), .alignment = Bytes(16) });
			return TargetABI{
				.triple      = TargetTriple{ .arch = Arch::X86_64, .os = OperatingSystem::Linux },
				.data_layout = DataLayout{
					.endianness        = Endianness::Little,
					.pointer_size      = Bytes(8),
					.pointer_alignment = Bytes(8),
					.float_layouts     = std::move(floats),
				},
			};
		}();
		return abi;
	}

	namespace {
		DataLayout aarch64DataLayout() {
			return DataLayout{
				.endianness        = Endianness::Little,
				.pointer_size      = Bytes(8),
				.pointer_alignment = Bytes(8),
				.float_layouts     = ieeeFloatLayouts(),
			};
		}
	}

	const TargetABI& aarch64Linux() {
		static const TargetABI abi = [] {
			return TargetABI{
				.triple      = TargetTriple{ .arch = Arch::AArch64, .os = OperatingSystem::Linux },
				.data_layout = aarch64DataLayout(),
			};
		}();
		return abi;
	}

	const TargetABI& aarch64Darwin() {
		static const TargetABI abi = [] {
			return TargetABI{
				.triple      = TargetTriple{ .arch = Arch::AArch64, .os = OperatingSystem::Darwin },
				.data_layout = aarch64DataLayout(),
			};
		}();
		return abi;
	}

	const TargetABI& hostTargetABI() {
		constexpr bool IS_X86_64  = base::IS_TARGET_ARCH_X86 && base::IS_TARGET_ARCH_64;
		constexpr bool IS_AARCH64 = base::IS_TARGET_ARCH_ARM && base::IS_TARGET_ARCH_64;

		static_assert(
			IS_X86_64 || IS_AARCH64,
			"abi::layout: unsupported host architecture; add a TargetABI preset for it"
		);

		if constexpr (IS_X86_64)
			return x86_64Linux();
		else if constexpr (base::IS_TARGET_OS_MACOS)
			return aarch64Darwin();
		else
			return aarch64Linux();
	}
}
