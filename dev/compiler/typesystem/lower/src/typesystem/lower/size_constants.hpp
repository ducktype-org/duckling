#pragma once

#include <base/bits_and_bytes.hpp>
#include <base/ints.hpp>

namespace tsl {
	// This may become const instead of constexpr because it might be defined during runtime.
	constexpr Bits META_SIZE    = base::bytes2bits(Bytes(8));
	constexpr Bits POINTER_SIZE = base::bytes2bits(Bytes(8));
	constexpr Bits BOOL_SIZE    = Bits(1);
	constexpr Bits BYTE_SIZE    = base::bytes2bits(Bytes(1));
	constexpr Bits CHAR_SIZE    = base::bytes2bits(Bytes(1));
}
