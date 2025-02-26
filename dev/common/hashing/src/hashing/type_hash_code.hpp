#pragma once

#include <type_traits>
#include <concepts>
#include <span>

#include <base/ints.hpp>

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
			TypeCodeBase<I> hash_value;

			template<std::size_t N>
			consteval StrToIntegral(const std::span<const char, N> span) {
				HashAlgorithm h;
				h(span);
				hash_value.value
					= static_cast<I>(static_cast<typename HashAlgorithm::result_type>(h));
			}

			consteval operator TypeCodeBase<I>() const { return hash_value; }
		};

		/**
		 * returns a unique string for each type
		 * uses implementation-specific macros to get unique strings
		 * (decorated function names contain the name of the type template parameters)
		 */
		template<
			typename,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval StrToIntegral<I, HashAlgorithm> uniqueString() {
#ifdef _MSC_VER
			return StrToIntegral<I, HashAlgorithm>{ std::span{ __FUNCDNAME__ } };
#elif defined(__GNUC__) || defined(__clang__)
			return StrToIntegral<I, HashAlgorithm>{ std::span{ __PRETTY_FUNCTION__ } };
#else
	#error "Please provide a unique string for each type"
			return { "" };
#endif
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
