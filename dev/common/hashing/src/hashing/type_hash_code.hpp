#pragma once

#include <type_traits>
#include <concepts>
#include <span>

#include <base/ints.hpp>
#include <base/type_traits.hpp>

#include "type_code.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {


	namespace detail {

		/**
		 * converts string to integral type using a given hash algorithm
		 */
		template<std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
		requires std::convertible_to<typename HashAlgorithm::result_type, I>
		struct StrToIntegral final {
			TypeCodeBase<I, false> hash_value;

			template<std::size_t N>
			consteval StrToIntegral(const std::span<std::byte, N> span) {
				HashAlgorithm h;
				h(span);
				hash_value.value = static_cast<I>(h.finalize());
			}

			consteval operator TypeCodeBase<I, false>() const { return hash_value; }
		};

		/**
		 * returns a unique string for each type
		 */
		template<
			typename T,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval StrToIntegral<I, HashAlgorithm> uniqueString() {
			constexpr std::string_view sv = base::typeName<T, false>();
			std::array<std::byte, sv.size()> byte_arr;
			for (std::size_t i = 0; i < sv.size(); ++i) 
				byte_arr[i] = static_cast<std::byte>(sv[i]);
			return StrToIntegral<I, HashAlgorithm>{ std::span{ byte_arr.begin(), byte_arr.end() } };
		}

		template<
			typename T,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval auto getIDFromUniqueString() {
			return static_cast<TypeCodeBase<I>>(uniqueString<T, I, HashAlgorithm>());
		}

	}  // namespace detail

	/**
	 * returns a hash code of a given length for the type
	 */
	template<
		typename T,
		std::integral I        = u32,
		typename HashAlgorithm = default_hash_algorithm_for<I>>
	static constexpr TypeCodeBase<I> TYPE_HASH_CODE
		= detail::getIDFromUniqueString<T, I, HashAlgorithm>();


}  // namespace hashing
