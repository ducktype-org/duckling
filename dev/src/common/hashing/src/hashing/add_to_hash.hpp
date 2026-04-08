#pragma once

#include "hash_algorithm_utils.hpp"

#include <base/comptime/type_traits.hpp>

#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace hashing {

	/**
	 * Options for the addToHash function template.
	 * Currently empty.
	 */
	struct AddToHashOptions final { };

	/**
	 * Default (strict) options for the addToHash function template
	 */
	static constexpr AddToHashOptions DEFAULT_ADD_TO_HASH_OPTIONS { };

	/**
	 * This is a template overload for the 'addToHash' function.
	 * If a friend function overload exists for the type, it will be used instead.
	 * This one serves as a fallback and a place where specializations for
	 * types that are not ours can be added (like the built-in types)
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam T - type of the object to hash
	 * @tparam Options - options for the addToHash function template
	 * @param hash_alg - hashing algorithm to use
	 * @param t - object to hash
	 */
	template<
		hash_algorithm HashAlgorithm,
		typename T,
		AddToHashOptions Options = DEFAULT_ADD_TO_HASH_OPTIONS>
	constexpr void addToHash(HashAlgorithm& hash_alg, const T& t) {
		// For most types we only want to add to hash some subset of their subobjects (bases +
		// members). This can be done easily by defining `hashDecompose` friend function that lists
		// subobjects in an order in which we want to hash them
		if constexpr (internal::can_hashDecompose<T>) {
			std::apply([&](auto&&... args) { (addToHash(hash_alg, args), ...); }, hashDecompose(t));
		}
		// Specializations for types that are not ours
		else if constexpr (std::is_floating_point_v<T>) {
			// IEEE 754 floating point numbers have multiple representations of 0:
			// -0.0 == 0.0, so they should have the same hash since they compare equal.
			// However, they are effectively a different group elements.
			auto t_copy = t;
			if (t_copy == 0) t_copy = 0;
			internal::hashAsBytes(hash_alg, t_copy);
		}
		// Specialization for pointers
		else if constexpr (std::is_pointer_v<T>) {
			internal::hashAsBytes(hash_alg, t);
		}
		// nullptr_t
		else if constexpr (std::is_null_pointer_v<T>) {
			internal::hashAsBytes(hash_alg, 0);
		}
		// If the range is contiguous and its elements have unique representations we can
		// treat it as a segment of memory and hash it directly
		else if constexpr (internal::can_hash_range_as_bytes<HashAlgorithm, T>) {
			internal::hashRangeAsBytes(hash_alg, t);
		}
		// If for each value of the type there is a unique representation of it in memory,
		// we can treat it as a sequence of chars and hash it directly
		else if constexpr (internal::can_hash_by_representation<T>) {
			internal::hashAsBytes(hash_alg, t);
		}
		// If type supports std::tuple_size and std::get, we can use them to get and hash its
		// members
		else if constexpr (internal::supports_std_get<T>) {
			[&]<std::size_t... I>(std::index_sequence<I...>) {
				(addToHash(hash_alg, std::get<I>(t)), ...);
			}(std::make_index_sequence<std::tuple_size_v<T>>{});
		}
		// Other overloads for ranges
		// Overload if range is contiguous
		else if constexpr (std::ranges::contiguous_range<T>) {
			for (const auto& elem: t) addToHash(hash_alg, elem);
		}
		else {
			// Note: the typeName(T) == "" is just to make the compiler print the T in the error message.
			static_assert(
				base::typeName<T> == "" && false, "Please provide an 'addToHash' or 'hashDecompose' overload for this type"
			);
		}
	}

	/**
	 * Variadic overload of the template addToHash() function
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam Ts - types of the objects to hash
	 * @param hash_alg - hashing algorithm to use
	 * @param ts - objects to hash
	 */
	template<hash_algorithm HashAlgorithm, typename... Ts>
	constexpr void addToHash(HashAlgorithm& hash_alg, const Ts&... ts) {
		(addToHash(hash_alg, ts), ...);
	}


}  // namespace hashing
