/**
 * @file bits_and_bytes.hpp
 *
 * @brief Defines dimensional types for storing a number of bits and a number of bytes,
 * particularly useful when e.g. talking about sizes of data types.
 */
#pragma once

#include <base/types/ints.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string>

STRONG_TYPEDEF_INT_DIMENSIONAL(Bits, usize);

STRONG_TYPEDEF_INT_DIMENSIONAL(Bytes, usize);

namespace base {
	constexpr Bits bytes2bits(Bytes bytes) { return Bits(usize(bytes) * 8); }
}

namespace base::internal {
	inline void strConcat(std::string& out, Bits bits) {
		out.append(std::to_string(usize(bits)) + "b");
	}

	inline void strConcat(std::string& out, Bytes bytes) {
		out.append(std::to_string(usize(bytes)) + "B");
	}
}
