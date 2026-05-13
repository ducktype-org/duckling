#pragma once

#include <base/types/ints.hpp>

namespace vm::fast {
	constexpr u64 encodePlace(bool as_global, u64 offset) {
		return (static_cast<u64>(as_global) << 63) | offset;
	}

	constexpr std::byte* decodePlace(
		u64 encoded, std::byte* local_stack_base, std::byte* global_buffer_base
	) {
		const bool is_global = encoded >> 63;
		const u64  offset    = encoded & ~(1ULL << 63);
		if (is_global)
			return global_buffer_base + offset;
		else
			return local_stack_base + offset;
	}
}
