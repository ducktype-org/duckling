/**
 * @file perfect_hash.hpp
 * @brief Provides a very simple framework for defining perfect hashing for any types.
 *
 * ### Usage
 *
 * In order to define perfect hash for a given type `T` you have to do one of:
 *
 * - write `t.customPerfectHash() const -> base::HashT` method.
 * - write `customPerfectHash(const T& [or T*]) -> base::HashT` function declared in the same
 * scope as type `T`.
 *
 * In order to get the perfect hash of any type use `base::perfectHash`.
 *
 * @include perfect_hash_example.cpp
 *
 * @example perfect_hash_example.cpp
 */
#pragma once

#include "ints.hpp"

#include <type_traits>
#include <concepts>

namespace base {

	using HashT = u64;

	/**
	 * @brief perfectHash for type u64
	 *
	 * @param v
	 * @return HashT
	 */
	inline HashT customPerfectHash(u64 v) { return v; }

	namespace detail {
		/**
		 * @brief Concept to check if given type
		 * has .perfectHash() method returning HashT
		 *
		 * @tparam T
		 */
		template<typename T>
		concept MemberHash = requires(T& t) {
			{ t.customPerfectHash() } -> std::same_as<HashT>;
		};

		/**
		 * @brief Concept to check if given type
		 * has perfectHash() function returning HashT
		 *
		 * @tparam T
		 */
		template<typename T>
		concept FunctionHash = requires(T& t) {
			{ customPerfectHash(t) } -> std::same_as<HashT>;
		};

		/**
		 * @brief Customization point that calls
		 * .perfectHash method if proper one exists or
		 * perfectHash function.
		 * The perfectHash function has to available via ADL of predefined here
		 *
		 * @tparam T - type to hash
		 * @param key - value to hash
		 * @return HashT
		 */
		template<class T>
		HashT perfectHashCPO(const T& key) {
			if constexpr (MemberHash<T>)
				return key.customPerfectHash();
			else if constexpr (FunctionHash<T>)
				return customPerfectHash(key);
			else {
				static_assert(
					!sizeof(T),
					"Actual error: Perfect hash for type T does not exist! T should be printed "
					"somewhere in the note bellow"
				);
				return 0;
			}
		}
	}

	/**
	 * @brief Obtain perfectHash of a value.
	 *
	 * @tparam T - type to hash
	 */
	template<class T>
	HashT perfectHash(const T& key) {
		return ::base::detail::perfectHashCPO(key);
	}

	template<class T>
	struct PerfectHashFunctor final {
		std::size_t operator()(const T& key) const { return ::base::perfectHash<T>(key); }
	};
}
