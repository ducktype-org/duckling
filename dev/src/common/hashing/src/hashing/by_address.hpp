// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "hash_algorithm_utils.hpp"

#include <type_traits>

namespace hashing {

	/**
	 * @brief Proxy type that makes hashing a pointer by the address it holds explicit.
	 *
	 * Raw pointers are deliberately not hashable on their own - hashing an address is almost
	 * never what one wants (addresses are not stable between runs and say nothing about the
	 * pointee, so e.g. a `const char*` would hash to something unrelated to the string it
	 * points to).
	 *
	 * @tparam Ptr - Type of pointer to wrap. Must be a pointer type.
	 */
	template<class Ptr>
	requires std::is_pointer_v<Ptr> class HashByAddress final {
		Ptr pointer;

	public:
		constexpr explicit HashByAddress(Ptr pointer) noexcept: pointer{ pointer } {}

		/**
		 * @brief Returns the wrapped pointer
		 * @return the pointer this proxy was constructed with
		 */
		[[nodiscard]]
		constexpr Ptr get() const noexcept {
			return pointer;
		}

		/**
		 * @brief Adds the held address to the hash
		 *
		 * @param hash_alg - hashing algorithm to use
		 * @param by_address - proxy holding the address to hash
		 */
		friend constexpr void addToHash(
			hash_algorithm auto& hash_alg, const HashByAddress& by_address
		) {
			internal::hashAsBytes(hash_alg, by_address.pointer);
		}
	};
}
