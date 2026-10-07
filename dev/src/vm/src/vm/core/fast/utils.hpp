// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

namespace vm::fast {
	constexpr u64 encodePlace(bool as_global, u64 offset) {
		return (static_cast<u64>(as_global) << 63) | offset;
	}

	constexpr byte* decodePlace(u64 encoded, byte* local_stack_base, byte* global_buffer_base) {
		const bool is_global = encoded >> 63;
		const u64  offset    = encoded & ~(1ULL << 63);
		if (is_global)
			return global_buffer_base + offset;
		else
			return local_stack_base + offset;
	}
}
