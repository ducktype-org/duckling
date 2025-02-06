#pragma once

#include <type_traits>
#include <concepts>
#include <utility>
#include <ranges>
#include <tuple>

#include "hash_algorithm_utils.hpp"

namespace hashing {


	/**
	 * this is a template overload for the 'addToHash' function
	 * if a friend function overload exists for the type, it will be used instead
	 * this one serves as a fallback and a place where specializations for
	 * types that are not ours can be added (like the built-in types)
	 */
	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void addToHash(HashAlgorithm& hash_alg, const T& t) {
		// for most types we only want to add to hash some subset of their subobjects (bases +
		// members) this can be done easily by defining `hashDecompose` friend function that lists
		// subobjects in an order in which we want to hash them
		if constexpr (detail::can_hashDecompose<T>) {
			std::apply([&](auto&&... args) { (addToHash(hash_alg, args), ...); }, hashDecompose(t));
		}
		// if there is no user-defined specialization for hashDecompose nor addToHash, but the
		// chosen hashing algorithm is able to hash the type directly, we can use it (for most
		// algorithms those will be types with unique representations)
		else if constexpr (detail::can_hash_directly<HashAlgorithm, T>) {
			hash_alg(t);
		}
		// specializations for types that are not ours
		else if constexpr (std::is_floating_point_v<T>) {
			// IEEE 754 floating point numbers have multiple representations of 0:
			// -0.0 == 0.0, but they should have the same hash
			auto t_copy = auto{ t };
			if (t_copy == 0) t_copy = 0;
			detail::hashAsChars(hash_alg, t_copy);
		}
		// specialation for pointers
		else if constexpr (std::is_pointer_v<T>) {
			detail::hashAsChars(hash_alg, t);
		}
		// nullptr_t
		else if constexpr (std::is_null_pointer_v<T>) {
			detail::hashAsChars(hash_alg, t);
		}
		// if the range is contiguous and its elements have unique representations we can
		// treat it as a segment of memory and hash it directly
		else if constexpr (detail::can_hash_range_as_chars<HashAlgorithm, T>) {
			detail::hashRangeAsChars(hash_alg, t);
		}
		// if type supports std::tuple_size and std::get, we can use them to get and hash its
		// members
		else if constexpr (detail::supports_std_get<T>) {
			[&]<std::size_t... I>(std::index_sequence<I...>) {
				(addToHash(hash_alg, std::get<I>(t)), ...);
			}(std::make_index_sequence<std::tuple_size_v<T>>{});
		}
		// other overloads for ranges
		// overload if range is contiguous
		else if constexpr (std::ranges::contiguous_range<T>) {
			for (auto&& elem: t) addToHash(hash_alg, elem);
		}
		// some ranges will compare equal but keep their elements in unspecified order
		else if constexpr (detail::can_hash_range_with_unspecified_order<HashAlgorithm, T>) {
			typename HashAlgorithm::result_type combined_result{};
			for (auto&& elem: t) {
				// note that this copy and hash finalization in cast may be expensive,
				// if possible the type should get a dedicated addToHash overload
				auto hash_copy = hash_alg;
				addToHash(hash_copy, elem);
				combined_result ^= static_cast<typename HashAlgorithm::result_type>(hash_copy);
			}
			addToHash(hash_alg, combined_result);
		}
		// std::hash is not constexpr, so if some type needs to be hashable in compile-time,
		// its specialization should be provided above
		else if constexpr (detail::can_stdhash<T>) {
			addToHash(hash_alg, std::hash<T>{}(t));
		} else {
			static_assert(
				false, "Please provide an 'addToHash' or 'hashDecompose' overload for this type"
			);
		}
	}

	/**
	 * variadic overload of template addToHash()
	 */
	template<hash_algorithm HashAlgorithm, typename... Ts>
	constexpr void addToHash(HashAlgorithm& hash_alg, const Ts&... ts) {
		(addToHash(hash_alg, ts), ...);
	}


}  // namespace hashing
