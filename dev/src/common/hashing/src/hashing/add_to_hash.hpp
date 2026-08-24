#pragma once

#include "by_address.hpp"
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
	struct AddToHashOptions final {};

	/**
	 * Default (strict) options for the addToHash function template
	 */
	static constexpr AddToHashOptions DEFAULT_ADD_TO_HASH_OPTIONS{};

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
	namespace cpo_detail {

		/**
		 * @brief ADL barrier.
		 *
		 * Unqualified lookup stops at the first enclosing scope that declares the name, so
		 * this declaration keeps the call inside the dispatcher from ever reaching
		 * `hashing::` - argument-dependent lookup is then the only way a hook can be found.
		 * It takes no arguments, so it is never a viable candidate itself; `= delete` makes a
		 * stray zero-argument call an error rather than a link failure.
		 */
		void addToHash() = delete;

		/**
		 * @brief Checks if the type hashes itself through a member `addToHash(alg)`.
		 */
		template<typename HashAlgorithm, typename T>
		concept has_member_addToHash = requires(HashAlgorithm& h, const T& t) { t.addToHash(h); };

		/**
		 * @brief Checks if a free `addToHash(alg, t)` for the type is reachable by ADL.
		 *
		 * Only ADL can answer this: the barrier above stops ordinary lookup, and the
		 * dispatcher is a variable, which ADL never considers. `T` is deduced from a real
		 * parameter, so the call is dependent and the lookup happens in the instantiation
		 * context - no tag parameter is needed the way `ser` needs one for `serMake`.
		 */
		template<typename HashAlgorithm, typename T>
		concept has_adl_addToHash = requires(HashAlgorithm& h, const T& t) { addToHash(h, t); };

		/**
		 * @brief The dispatcher behind `hashing::addToHash`.
		 *
		 * This is a class, not a function template, on purpose. `hashing::addToHash` is
		 * therefore a variable, and ADL considers only functions and function templates - so
		 * the dispatcher can never find itself through the hash algorithm's namespace. Were
		 * it a function template, `has_adl_addToHash` would be satisfied for every type
		 * (ADL reaches `hashing` through the algorithm argument) and the ADL branch below
		 * would recurse forever.
		 *
		 * A variable found by ordinary lookup also suppresses ADL at the call site, so every
		 * entry point - qualified, unqualified, from any namespace - lands here and gets the
		 * same dispatch.
		 */
		struct addToHash_fn final {
			/**
			 * @brief Adds a single object to the hash.
			 *
			 * @tparam HashAlgorithm - type of the hashing algorithm to use
			 * @tparam T - type of the object to hash
			 * @param hash_alg - hashing algorithm to use
			 * @param t - object to hash
			 */
			template<
				hash_algorithm HashAlgorithm,
				typename T,
				AddToHashOptions Options = DEFAULT_ADD_TO_HASH_OPTIONS>
			constexpr void operator()(HashAlgorithm& hash_alg, const T& t) const {
				// An explicit hook always wins over any automatic path. Member first: it is
				// found by member lookup, so no ADL games are involved.
				if constexpr (has_member_addToHash<HashAlgorithm, T>) {
					t.addToHash(hash_alg);
				}
				// A free hook - a hidden friend or a function in the type's namespace.
				// This is the one call that must stay unqualified: the barrier blocks
				// ordinary lookup so only ADL can supply a candidate.
				else if constexpr (has_adl_addToHash<HashAlgorithm, T>) {
					addToHash(hash_alg, t);
				}
				// For most types we only want to add to hash some subset of their subobjects
				// (bases + members). This can be done easily by defining `hashDecompose`
				// friend function that lists subobjects in an order in which we want to hash
				// them
				else if constexpr (internal::can_hashDecompose<T>) {
					std::apply(
						[&](auto&&... args) {
							internal::hashCompositeArity<sizeof...(args)>(hash_alg);
							((*this)(hash_alg, args), ...);
						},
						hashDecompose(t)
					);
				}
				// Specializations for types that are not ours
				else if constexpr (std::is_floating_point_v<T>) {
					// A floating point type wider than its value bits carries padding, and
					// padding is indeterminate - hashing it would give the same value two
					// different hashes. x87 `long double` is 10 bytes of value in 16.
					static_assert(
						std::has_unique_object_representations_v<T> || sizeof(T) <= 8,
						"This floating point type has padding bytes, so hashing its object "
						"representation is not reproducible. Hash the significant bytes "
						"explicitly instead"
					);
					// IEEE 754 floating point numbers have multiple representations of 0:
					// -0.0 == 0.0, so they should have the same hash since they compare equal.
					auto t_copy = t;
					if (t_copy == 0) t_copy = 0;
					internal::hashAsBytes(hash_alg, t_copy);
				}
				// Pointers are intentionally not hashable on their own - hashing an address is
				// almost never what one wants, so the intent has to be spelled out with the
				// HashByAddress proxy
				else if constexpr (std::is_pointer_v<T>) {
					static_assert(
						false,
						"Raw pointers are not hashed implicitly. Hash the pointee ('*ptr'), or "
						"wrap the pointer in 'hashing::HashByAddress{ ptr }' to hash the address "
						"itself"
					);
				}
				// nullptr_t
				else if constexpr (std::is_null_pointer_v<T>) {
					internal::hashAsBytes(hash_alg, 0);
				}
				// If for each value of the type there is a unique representation of it in
				// memory, we can treat it as a sequence of chars and hash it directly
				else if constexpr (internal::can_hash_by_representation<T>) {
					internal::hashAsBytes(hash_alg, t);
				}
				// If the range is contiguous and its elements have unique representations we
				// can treat it as a segment of memory and hash it directly
				else if constexpr (internal::can_hash_range_as_bytes<HashAlgorithm, T>) {
					internal::hashRangeAsBytes(hash_alg, t);
				}
				// If type supports std::tuple_size and std::get, we can use them to get and
				// hash its members
				else if constexpr (internal::supports_std_get<T>) {
					[&]<std::size_t... I>(std::index_sequence<I...>) {
						internal::hashCompositeArity<sizeof...(I)>(hash_alg);
						((*this)(hash_alg, std::get<I>(t)), ...);
					}(std::make_index_sequence<std::tuple_size_v<T>>{});
				}
				// Other overloads for ranges
				// Overload if range is contiguous
				else if constexpr (std::ranges::contiguous_range<T>) {
					internal::hashRangeLengthPrefix(hash_alg, std::ranges::size(t));
					for (const auto& elem: t) (*this)(hash_alg, elem);
				} else {
					static_assert(
						false,
						"Please provide an 'addToHash' or 'hashDecompose' overload for this type"
					);
				}
			}

			/**
			 * @brief Adds several objects to the hash, in order.
			 *
			 * @tparam HashAlgorithm - type of the hashing algorithm to use
			 * @tparam Ts - types of the objects to hash
			 * @param hash_alg - hashing algorithm to use
			 * @param ts - objects to hash
			 */
			template<hash_algorithm HashAlgorithm, typename... Ts>
			requires(sizeof...(Ts) != 1)
			constexpr void operator()(HashAlgorithm& hash_alg, const Ts&... ts) const {
				((*this)(hash_alg, ts), ...);
			}
		};

	}  // namespace cpo_detail

	/**
	 * @brief Adds an object (or several) to a hash algorithm's state.
	 *
	 * Dispatches, in order, to: a member `addToHash(alg)`, a free `addToHash(alg, t)` found
	 * by ADL, `hashDecompose(t)`, then the built-in paths for floating point types, ranges
	 * and tuple-likes. A type that fits none of them is a compile error naming what to add.
	 *
	 * This is a function object rather than a function template so that it cannot be found
	 * by ADL - see cpo_detail::addToHash_fn.
	 */
	// This is a callable that replaced a function of the same name, so it keeps the function
	// spelling rather than the constant one - renaming it would break every call site.
	// NOLINTNEXTLINE(readability-identifier-naming)
	inline constexpr cpo_detail::addToHash_fn addToHash{};


}  // namespace hashing
