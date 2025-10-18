#pragma once

#include "hashing_algorithms.hpp"
#include "type_code.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>

#include <concepts>
#include <span>

namespace hashing {


	namespace internal {

		/**
		 * Type that converts string to integral type using a given hash algorithm
		 *
		 * @tparam I - type of the value of the type code
		 * @tparam HashAlgorithm - type of the hashing algorithm to use to get the hash code
		 */
		template<std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
		requires std::convertible_to<typename HashAlgorithm::result_type, I>
		struct StrToIntegral final {
			TypeCode<I, false> hash_value;

			consteval StrToIntegral(const std::span<const std::byte> span) {
				HashAlgorithm h;
				h(span);
				hash_value.value = static_cast<I>(h.finalize());
			}

			consteval operator TypeCode<I, false>() const { return hash_value; }
		};

		/**
		 * Returns a unique string for each type
		 *
		 * @tparam T - type to get the unique string for
		 * @tparam I - type of the value of the type code
		 * @tparam HashAlgorithm - type of the hashing algorithm to use to get the hash code
		 */
		template<
			typename T,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval StrToIntegral<I, HashAlgorithm> uniqueString() {
			constexpr std::string_view       sv = base::typeName<T, false>();
			std::array<std::byte, sv.size()> byte_arr;
			for (std::size_t i = 0; i < sv.size(); ++i) byte_arr[i] = static_cast<std::byte>(sv[i]);
			return StrToIntegral<I, HashAlgorithm>{ std::span{ byte_arr.begin(), byte_arr.end() } };
		}

		/**
		 * Returns a unique hash code of a given length for the type using a given hash algorithm
		 *
		 * @tparam T - type to get the unique hash code for
		 * @tparam I - type of the value of the type code
		 * @tparam HashAlgorithm - type of the hashing algorithm to use to get the hash code
		 */
		template<
			typename T,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval auto getIDFromUniqueString() {
			return static_cast<TypeCode<I, false>>(uniqueString<T, I, HashAlgorithm>());
		}

	}  // namespace internal

	/**
	 * Returns a hash code of a given length for the type
	 *
	 * @tparam T - type to get the hash code for
	 * @tparam I - type of the value of the type code
	 * @tparam HashAlgorithm - type of the hashing algorithm to use to get the hash code
	 */
	template<typename T, std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
	static constexpr TypeCode<I, false> TYPE_HASH_CODE
		= internal::getIDFromUniqueString<T, I, HashAlgorithm>();


}  // namespace hashing
