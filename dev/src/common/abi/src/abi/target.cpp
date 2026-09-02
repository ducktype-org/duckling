#include <abi/target.hpp>

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
#if defined(__x86_64__) || defined(_M_X64)
		return x86_64Linux();
#elif defined(__aarch64__) || defined(_M_ARM64)
	#if defined(__APPLE__)
		return aarch64Darwin();
	#else
		return aarch64Linux();
	#endif
#else
	#error "abi::layout: unsupported host architecture; add a TargetABI preset for it"
#endif
	}

}
