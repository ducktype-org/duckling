#pragma once

#include <base/ints.hpp>
#include <base/bits_and_bytes.hpp>

namespace tsl {
	// This may become const instead of constexpr because it might be defined during runtime.
	constexpr Bits META_SIZE    = B2b(Bytes(8));
	constexpr Bits POINTER_SIZE = B2b(Bytes(8));
	constexpr Bits BYTE_SIZE    = B2b(Bytes(1));
	constexpr Bits BOOL_SIZE    = BYTE_SIZE;
	constexpr Bits CHAR_SIZE    = BYTE_SIZE;
}
