#pragma once

#include <concepts>
#include <type_traits>
#include <string_view>

#include <base/ints.hpp>

#include "type_hash_code_def.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {


	namespace detail {

		// converts string to integral type using a given hash algorithm
		template<std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
		requires std::convertible_to<typename HashAlgorithm::result_type, I> struct StrToIntegral {
			TypeHashCodeBase<I> hash_value;

			consteval StrToIntegral(const std::string_view sv) {
				HashAlgorithm h;
				h(sv);
				hash_value.value = static_cast<typename HashAlgorithm::result_type>(h);
			}

			consteval operator TypeHashCodeBase<I>() const { return hash_value; }
		};

		// returns a unique string for each type
		// uses implementation-specific macros to get unique strings
		// (decorated function names contain the name of the type template parameters)
		template<
			typename,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval StrToIntegral<I, HashAlgorithm> uniqueString() {
#ifdef _MSC_VER
			return StrToIntegral<I, HashAlgorithm>{ __FUNCDNAME__ };
#elifndef __PRETTY_FUNCTION__
			return StrToIntegral<I, HashAlgorithm>{ __PRETTY_FUNCTION__ };
#else
	#error "Please provide a unique string for each type"
			return { "" };
#endif
		}

		template<
			typename T,
			std::integral I        = u32,
			typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval auto uniqueId() {
			return static_cast<TypeHashCodeBase<I>>(uniqueString<T, I, HashAlgorithm>());
		}

	}  // namespace detail

	// returns a unique hash code of a given length for a type
	template<
		typename T,
		std::integral I        = u32,
		typename HashAlgorithm = default_hash_algorithm_for<I>>
	static constexpr TypeHashCodeBase<I> TYPE_HASH_CODE = detail::uniqueId<T, I, HashAlgorithm>();


}  // namespace hashing
