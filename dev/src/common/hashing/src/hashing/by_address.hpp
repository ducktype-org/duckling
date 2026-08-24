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
		 *
		 * @note A member and not a friend on purpose: this class lives in namespace
		 * `hashing`, and a hidden friend would inject the name `addToHash` there, clashing
		 * with the `hashing::addToHash` dispatcher object.
		 */
		constexpr void addToHash(hash_algorithm auto& hash_alg) const {
			internal::hashAsBytes(hash_alg, pointer);
		}
	};
}
