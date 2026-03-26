#pragma once

#include "add_to_hash.hpp"
#include "hashing_algorithms.hpp"

#include <base/comptime/type_traits.hpp>

#include <type_traits>

namespace hashing {


	namespace internal {

		/**
		 * Checks if a given type has a nested value_type member type
		 */
		template<class T>
		concept has_value_type = requires { typename T::value_type; };


	}  // namespace internal

	/**
	 * @brief Callable type that obtains a hash for an object it is called with
	 * together with it's type code using the specified hash algorithm
	 *
	 * @tparam HashAlgorithm - Hashing algorithm to use
	 * @tparam TypeC - Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<hash_algorithm HashAlgorithm = DefaultHashAlgorithm>
	class Hash final {
	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const {
			HashAlgorithm h{};

			addToHash(h, t);

			return h.finalize();
		}
	};

	/**
	 * @brief Callable type that obtains hash values for sequences of objects it is called with
	 * Type keeps the state between calls, so next objects can be appended.
	 * Calling finalize() yields the hash value corresponding to the current state
	 *
	 * @tparam HashAlgorithm - Hashing algorithm to use
	 * @tparam TypeC - Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<hash_algorithm HashAlgorithm = DefaultHashAlgorithm>
	class StatefulHash final {
		HashAlgorithm h{};

	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr StatefulHash& operator()(const T& t) {
			addToHash(h, t);

			return *this;
		}

		template<typename... Ts>
		constexpr StatefulHash& operator()(const Ts&... ts) noexcept {
			return ((this->operator()(ts)), ...);
		}

		[[nodiscard]]
		constexpr result_type finalize() noexcept {
			return h.finalize();
		}
	};

	/**
	 * @brief Gets the hash value for the object using the specified hash algorithm
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam TypeC - type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 * @param t - object to hash
	 * @return hash value
	 */
	template<hash_algorithm HashAlgorithm = DefaultHashAlgorithm>
	auto justHash(const auto& t) {
		return Hash<HashAlgorithm>{}(t);
	}

	/**
	 * @brief Variadic version of justHash(), uses StatefulHash to hash multiple objects
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam TypeC - type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 * @param ts - objects to hash
	 * @return hash value
	 */
	template<hash_algorithm HashAlgorithm = DefaultHashAlgorithm>
	auto justHash(const auto&... ts) {
		return StatefulHash<HashAlgorithm>{}(ts...).finalize();
	}


}  // namespace hashing
