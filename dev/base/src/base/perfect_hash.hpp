#pragma once

#include "ints.hpp"
#include <type_traits>

namespace base {

	using HashT = u64;

	/**
	 * @brief perfectHash for type u64
	 * 
	 * @param v 
	 * @return HashT 
	 */
	static HashT perfectHash(u64 v) { return v; }

	namespace detail {
		/**
		 * @brief Concept to check if given type
		 * has .perfectHash() method returning HashT
		 * 
		 * @tparam T 
		 */
		template<typename T>
      	concept MemberHash = requires(T& t) {
			{ t.perfectHash() } -> std::same_as<HashT>;
		};

		/**
		 * @brief Customization point that calls
		 * .perfectHash method if proper one exists or
		 * perfectHash function.
		 * The perfectHash function has to be visible from point
		 * of template instantiation or via ADL.
		 * 
		 * @tparam T - type to hash
		 * @param key - value to hash
		 * @return HashT 
		 */
		template<class T>
		HashT perfectHashCPO(const T& key) {
			if constexpr (MemberHash<T>) {
				return key.perfectHash();
			}
			else {
				return perfectHash(key);
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
}

