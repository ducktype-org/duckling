/**
 * @file bits_and_bytes.hpp
 *
 * @brief Defines dimensional types for storing a number of bits and a number of bytes,
 * particularly useful when e.g. talking about sizes of data types.
 */
#pragma once

#include "strongly_typed_int.hpp"
#include "ints.hpp"

STRONG_TYPEDEF_INT_DIMENSIONAL(Bits, usize);

STRONG_TYPEDEF_INT_DIMENSIONAL(Bytes, usize);

namespace base {
	inline constexpr Bits bytes2bits(Bytes bytes) { return Bits(usize(bytes) * 8); }
}

namespace std {
	inline std::string to_string(Bits bits) { return to_string(usize(bits)) + "b"; }

	inline std::string to_string(Bytes bytes) { return to_string(usize(bytes)) + "B"; }
}
