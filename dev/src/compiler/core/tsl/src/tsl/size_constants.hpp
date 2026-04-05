#pragma once

#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

namespace compiler::tsl {
	// This may become const instead of constexpr because it might be defined during runtime.

	// Size constants in bytes.
	constexpr Bytes META_SIZE_BYTES    = Bytes(8);
	constexpr Bytes POINTER_SIZE_BYTES = Bytes(8);
	constexpr Bytes BYTE_SIZE_BYTES    = Bytes(1);
	constexpr Bytes CHAR_SIZE_BYTES    = Bytes(1);

	// Size constants in bits.
	// Calculated based on the byte size constants.
	constexpr Bits META_SIZE    = base::bytes2bits(META_SIZE_BYTES);
	constexpr Bits POINTER_SIZE = base::bytes2bits(POINTER_SIZE_BYTES);
	constexpr Bits BOOL_SIZE    = Bits(1);
	constexpr Bits BYTE_SIZE    = base::bytes2bits(BYTE_SIZE_BYTES);
	constexpr Bits CHAR_SIZE    = base::bytes2bits(CHAR_SIZE_BYTES);
}
