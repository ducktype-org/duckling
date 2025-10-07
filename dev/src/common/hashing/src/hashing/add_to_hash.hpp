#pragma once

#include "hash_algorithm_utils.hpp"

#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace hashing {

	/**
	 * Options for the addToHash function template:
	 * * allow_std_hash - if true, the function will try to use std::hash for hashing types that
	 *     can't be hashed using the provided hashing algorithm
	 * * allow_hashing_ranges_with_unspecified_order - if true, the function will hash ranges that
	 *     may have unspecified order of elements, which circumvents the strict use of the hashing
	 *     algorithm
	 */
	struct AddToHashOptions {
		bool allow_std_hash;
		bool allow_hashing_ranges_with_unspecified_order;
	};

	/**
	 * Default (strict) options for the addToHash function template
	 */
	static constexpr AddToHashOptions DEFAULT_ADD_TO_HASH_OPTIONS{
		.allow_std_hash = false, .allow_hashing_ranges_with_unspecified_order = false
	};

	/**
	 * Relaxed options for the addToHash function template
	 */
	static constexpr AddToHashOptions RELAXED_ADD_TO_HASH_OPTIONS{
		.allow_std_hash = true, .allow_hashing_ranges_with_unspecified_order = true
	};

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
			// -0.0 == 0.0, but they should have the same hash since they compare equal
			auto t_copy = t;
			if (t_copy == 0) t_copy = 0;
			internal::hashAsBytes(hash_alg, t_copy);
		}
		// Specialation for pointers
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
		else if constexpr (std::has_unique_object_representations_v<T>) {
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
			addToHash(hash_alg, std::ranges::size(t));
			for (const auto& elem: t) addToHash(hash_alg, elem);
		}
		// Some ranges will compare equal but keep their elements in unspecified order
		else if constexpr (Options.allow_hashing_ranges_with_unspecified_order
		                   && internal::can_hash_range_with_unspecified_order<HashAlgorithm, T>) {
			typename HashAlgorithm::result_type combined_result{};
			for (auto&& elem: t) {
				// Note that this copy and hash finalization in cast may be expensive,
				// if possible the type should get a dedicated addToHash overload
				auto hash_copy = hash_alg;
				addToHash(hash_copy, elem);
				combined_result ^= static_cast<typename HashAlgorithm::result_type>(hash_copy);
			}
			addToHash(hash_alg, combined_result);
		}
		// std::hash is not constexpr, so if some type needs to be hashable in compile-time,
		// its specialization should be provided above
		else if constexpr (Options.allow_std_hash && internal::can_stdhash<T>) {
			addToHash(hash_alg, std::hash<T>{}(t));
		} else {
			static_assert(
				false, "Please provide an 'addToHash' or 'hashDecompose' overload for this type"
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
