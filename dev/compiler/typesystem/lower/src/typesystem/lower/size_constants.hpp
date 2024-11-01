#pragma once

#include <base/ints.hpp>
#include <base/bits_and_bytes.hpp>

namespace tsl {
	// This may become const instead of constexpr because it might be defined during runtime.
	constexpr Bits META_SIZE    = base::bytes2bits(Bytes(8));
	constexpr Bits POINTER_SIZE = base::bytes2bits(Bytes(8));
	constexpr Bits BYTE_SIZE    = base::bytes2bits(Bytes(1));
	constexpr Bits BOOL_SIZE    = BYTE_SIZE;
	constexpr Bits CHAR_SIZE    = BYTE_SIZE;
}
