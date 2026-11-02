// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file hash_mix.hpp
 * @brief Cheap bit mixers for in-memory hash values.
 */
#pragma once

#include <base/types/ints.hpp>

namespace base {
	/**
	 * @brief Mixes all bits of @p x into all bits of the result (murmur3 fmix64 finalizer).
	 *
	 * Use it to turn structured values (sequential ids, pointers, XOR-combined words) into
	 * well distributed hash values. It is a bijection, so distinct inputs never collide.
	 *
	 * @note In-memory hashing only: the result must never be persisted.
	 */
	[[nodiscard]]
	constexpr u64 hashMix64(u64 x) noexcept {
		x ^= x >> 33;
		x *= 0xff51afd7ed558ccdULL;
		x ^= x >> 33;
		x *= 0xc4ceb9fe1a85ec53ULL;
		x ^= x >> 33;
		return x;
	}

	/**
	 * @brief Combines an already mixed @p seed with another value @p value.
	 * Order dependent, so (a, b) and (b, a) give different results.
	 */
	[[nodiscard]]
	constexpr u64 hashCombine64(u64 seed, u64 value) noexcept {
		return hashMix64(seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)));
	}
}
