#pragma once

#include "hash_algorithm_utils.hpp"

#include <type_traits>

namespace hashing {

	namespace internal {

		/**
		 * Checks if the type is a raw pointer
		 */
		template<typename T>
		concept raw_pointer = std::is_pointer_v<T>;

	}  // namespace internal

	/**
	 * @brief Proxy type that makes hashing a pointer by the address it holds explicit.
	 *
	 * Raw pointers are deliberately not hashable on their own - hashing an address is almost
	 * never what one wants (addresses are not stable between runs and say nothing about the
	 * pointee, so e.g. a `const char*` would hash to something unrelated to the string it
	 * points to). Wrapping a pointer in this proxy states that hashing the address itself,
	 * i.e. the identity of the pointee, is intended.
	 *
	 * ~~~~~cpp
	 * justHash(ptr);                 // ill-formed
	 * justHash(*ptr);                // hashes the pointee
	 * justHash(ByAddress{ ptr });    // hashes the address
	 * ~~~~~
	 *
	 * @tparam Ptr - the wrapped pointer type
	 */
	template<internal::raw_pointer Ptr>
	class ByAddress final {
		Ptr pointer;

	public:
		/**
		 * @brief Wraps a pointer so that it is hashed by the address it holds
		 *
		 * @param pointer - pointer whose address should be hashed
		 */
		constexpr explicit ByAddress(Ptr pointer) noexcept: pointer{ pointer } {}

		/**
		 * @brief Returns the wrapped pointer
		 *
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
		friend constexpr void addToHash(hash_algorithm auto& hash_alg, const ByAddress& by_address) {
			internal::hashAsBytes(hash_alg, by_address.pointer);
		}
	};


}  // namespace hashing
