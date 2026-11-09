// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file bits_and_bytes.hpp
 *
 * @brief Defines dimensional types for storing a number of bits and a number of bytes,
 * particularly useful when e.g. talking about sizes of data types.
 */
#pragma once

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <string>

STRONG_TYPEDEF_INT_DIMENSIONAL(Bits, usize);

STRONG_TYPEDEF_INT_DIMENSIONAL(Bytes, usize);

STRONGLY_TYPED_INT_STD_HASH(Bytes)

STRONGLY_TYPED_INT_STD_HASH(Bits)

namespace base {
	constexpr Bits bytes2bits(Bytes bytes) { return Bits(usize(bytes) * 8); }

	constexpr Bytes bits2bytes(Bits bits) {
		CORE_ASSERT(
			usize(bits) % 8 == 0,
			"Bits must be divisible by 8 to be converted to bytes without a loss of precision"
		);
		return Bytes(usize(bits) / 8);
	}

	constexpr Bytes bits2bytesRoundUp(Bits bits) { return Bytes((usize(bits) + 7) / 8); }

	constexpr Bytes bytesRoundTo(Bytes size, Bytes round_to) {
		return ((size + round_to - Bytes(1)) / round_to.asInt()) * round_to.asInt();
	}
}

namespace base::internal {
	constexpr void strConcat(std::string& out, Bits bits) {
		out.append(std::to_string(usize(bits)) + "b");
	}

	constexpr void strConcat(std::string& out, Bytes bytes) {
		out.append(std::to_string(usize(bytes)) + "B");
	}
}
