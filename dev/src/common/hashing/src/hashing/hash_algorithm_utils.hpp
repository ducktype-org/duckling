// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>
#include <base/types/monostate.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace hashing {


	namespace internal {

		/**
		 * checks if the type is a span of const or non-const bytes.
		 * @note Implementation is weird to handle the length argument of span correctly.
		 */
		template<typename T>
		concept span_of_bytes
			= base::IsInstantiationOfTypeValue<T, std::span>
		   && std::same_as<std::remove_const_t<typename T::value_type>, std::byte>;

		/**
		 * checks if the type can be invoked with a span of byte
		 */
		template<typename T>
		concept invocable_with_byte_span
			= requires(T t) { t(std::declval<std::span<const std::byte>>()); };

		/**
		 * Checks if the type has a finalize() method that returns a result_type
		 */
		template<typename T>
		concept has_finalize = requires(T t) {
			{ t.finalize() } -> std::same_as<typename T::result_type>;
		};

		/**
		 * Implementation of the hash_algorithm concept
		 */
		template<typename T>
		concept hash_algorithm_impl
			= std::is_object_v<T> && std::is_constructible_v<T> && std::is_destructible_v<T>
		   && requires { typename T::result_type; }
		   && invocable_with_byte_span<T> && has_finalize<T>;

	}  // namespace internal

	/**
	 * Checks if the type is a hash algorithm
	 */
	template<typename T>
	concept hash_algorithm = internal::hash_algorithm_impl<std::remove_cvref_t<T>>;

	/**
	 * Returns a reference to one of the Bases of Derived
	 *
	 * @tparam Base - the base class to get a reference to
	 * @param derived - object for which we want to get a reference to the base
	 * @return reference to (one of) the base(s) of derived
	 */
	template<typename Base, std::derived_from<std::remove_cvref_t<Base>> Derived>
	requires(not std::is_same_v<std::remove_cvref_t<Base>, std::remove_cvref_t<Derived>>)
	constexpr const Base& getBase(const Derived& derived) noexcept {
		return static_cast<const Base&>(derived);
	}

	namespace internal {
		/**
		 * Checks if the type can be hashed by just hashing its representation.
		 *
		 * @note See HashByAddress for hashing pointers by their address.
		 */
		template<typename T>
		concept can_hash_by_representation
			= std::has_unique_object_representations_v<T> && (not std::is_pointer_v<T>)
		   && (std::is_integral_v<T> || std::is_enum_v<T> ||

		       // the const Monostate& here is needed, as this is
		       // simply how it works with static constexpr members.
		       requires {
				   {
					   T::HASHING_CAN_HASH_BY_REPRESENTATION
				   } -> std::same_as<const base::Monostate&>;
			   });

		/**
		 * Checks if the type is a tuple of references
		 */
		template<typename T>
		concept tuple_of_refs = requires(T t) {
			[]<typename... Args>(std::tuple<Args...>)
				requires std::is_same_v<std::tuple<Args...>, T>
			          && (std::is_reference_v<Args> && ...) {}(t);
		};

		/**
		 * Checks if hashDecompose() can be called on the type and if it returns a tuple of
		 * references
		 */
		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> tuple_of_refs;
		};

		/**
		 * Hashes an object as a sequence of bytes
		 */
		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsBytes(HashAlgorithm& h, const T& t) {
			const auto      arr = std::bit_cast<std::array<const std::byte, sizeof(T)>, T>(t);
			const std::span span{ arr.data(), arr.size() };
			h(span);
		}

		/**
		 * Checks if a range can be hashed as a contiguous sequence of memory
		 * (i.e. its elements are in a contiguous memory block, individual elements meet
		 * can_hash_by_representation and range size is known)
		 */
		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_bytes
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && can_hash_by_representation<std::ranges::range_value_t<R>>
		   && requires(const R& r) { std::ranges::size(r); };

		/**
		 * @brief Checks if the type hashes itself through a member `addToHash(alg)`.
		 *
		 * A member is found by member lookup, which has nothing to do with
		 * argument-dependent lookup - so unlike a free hook, it is seen no matter how the
		 * call at the top of the chain was spelled.
		 */
		template<typename HashAlgorithm, typename T>
		concept has_member_addToHash = hash_algorithm<HashAlgorithm>
		                            && requires(HashAlgorithm& h, const T& t) { t.addToHash(h); };

		/**
		 * @brief Adds a composite's member count to the hash.
		 *
		 * The length prefix below makes a variable-length field self-delimiting, but a
		 * composite still is not: {int, int, float} and {float, std::string} both flatten to
		 * twelve zero bytes when default constructed, so two unrelated types would share a
		 * hash. The member count in front separates them.
		 *
		 * One byte, not eight: the count is known at compile time and tiny, while a small
		 * object is only a handful of bytes of payload - a wider prefix would be most of what
		 * the algorithm ever sees.
		 *
		 * This does not make the hash type-aware. See the readme: equal arity and equal field
		 * widths still agree, and a hand-written hook that does not go through
		 * `hashDecompose` gets no count at all.
		 */
		template<std::size_t MEMBERS, hash_algorithm HashAlgorithm>
		constexpr void hashCompositeArity(HashAlgorithm& h) {
			static_assert(MEMBERS <= 0xFF, "a composite with more than 255 hashed members");
			hashAsBytes(h, static_cast<unsigned char>(MEMBERS));
		}

		/**
		 * @brief Adds a range's element count to the hash.
		 *
		 * Without it the byte stream is not self-delimiting: two variable-length fields
		 * hashed one after another cannot be told from the same bytes split differently, so
		 * ("ab", "c") and ("a", "bc") collide.
		 */
		template<hash_algorithm HashAlgorithm>
		constexpr void hashRangeLengthPrefix(HashAlgorithm& h, std::size_t size) {
			// u64 on purpose, not std::size_t - the hash must not depend on the platform's
			// pointer width.
			hashAsBytes(h, static_cast<u64>(size));
		}

		/**
		 * Hashes a range as a contiguous sequence of memory
		 * (requires that its elements are in a contiguous memory block, have unique object
		 * representations and size is known)
		 */
		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		requires can_hash_range_as_bytes<HashAlgorithm, R>
		constexpr void hashRangeAsBytes(HashAlgorithm& h, const R& r) {
			constexpr std::size_t ELEM_SIZE   = sizeof(std::ranges::range_value_t<R>);
			const std::size_t     r_size      = std::ranges::size(r);
			const std::size_t     buffer_size = r_size * ELEM_SIZE;

			hashRangeLengthPrefix(h, r_size);

			if consteval {
				// In a constant expression the only way to read an object's memory
				// representation is std::bit_cast, so the elements are copied one by one into
				// a buffer. Since C++20 an allocation freed in the same expression it was
				// allocated in is allowed in a constant expression.
				auto* const buffer = ::new std::byte[buffer_size];

				for (u64 i = 0, j = 0; i < r_size; ++i, j += ELEM_SIZE) {
					// Ranges may have both singed and unsigned index types and there is not
					// good trait that can always tell which one the range expects. To suppress
					// warnings we get the elements using std::next with range's
					// difference_type
					const auto& elem = *std::next(
						std::ranges::begin(r), static_cast<std::ranges::range_difference_t<R>>(i)
					);
					const auto arr = std::bit_cast<std::array<const std::byte, ELEM_SIZE>>(elem);
					std::copy(arr.begin(), arr.end(), buffer + j);
				}

				h(std::span<const std::byte>{ buffer, buffer_size });

				delete[] buffer;
			} else {
				// At run time the buffer is pure overhead - the concept above already requires
				// a contiguous range, so the bytes can be handed over in place. Measured: the
				// buffer cost one heap allocation and one full copy per call, at -O0 and -O2
				// alike, despite the copy elision the old comment here hoped for.
				h(std::as_bytes(std::span{ std::ranges::data(r), r_size }));
			}
		}

		/**
		 * Checks if the type is tuple-like i.e. supports std::tuple_size and std::get
		 */
		template<typename T>
		concept supports_std_get = requires {
			// Don't remove this line and don't change to std::tuple_size_v.
			// If std::tuple_size_v is ill-formed, the fail may happen not in the immediate context
			// of the concept check which may omit SFINAE and cause a hard error
			std::tuple_size<T>::value;

			[]<std::size_t... Is>(std::index_sequence<Is...>) requires requires {
				(std::get<Is>(std::declval<T>()), ...);
			} {}(std::make_index_sequence<
				 /* don't change to std::tuple_size_v */ std::tuple_size<T>::value>{});
		};

	}  // namespace internal


}  // namespace hashing
