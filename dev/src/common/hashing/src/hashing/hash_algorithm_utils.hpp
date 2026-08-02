#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>
#include <base/types/monostate.hpp>

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
		 * Checks if the type can be hashed by just hashing its representation
		 */
		template<typename T>
		concept can_hash_by_representation = std::has_unique_object_representations_v<T>
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
		 * (i.e. its elements are in a contiguous memory block, have unique object representations
		 * and size is known)
		 *
		 * @note ranges of raw pointers are excluded on purpose - hashing them as bytes would
		 * silently hash the addresses, which has to be requested explicitly with the HashByAddress
		 * proxy instead
		 */
		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_bytes
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && can_hash_by_representation<std::ranges::range_value_t<R>>
		   && (not std::is_pointer_v<std::ranges::range_value_t<R>>)
		   && requires(const R& r) { std::ranges::size(r); };

		/**
		 * Hashes a range as a contiguous sequence of memory
		 * (requires that its elements are in a contiguous memory block, have unique object
		 * representations and size is known)
		 */
		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		requires can_hash_range_as_bytes<HashAlgorithm, R>
		constexpr void hashRangeAsBytes(HashAlgorithm& h, const R& r) {
			// In C++23 the only way to get the memory representation of an object
			// as a sequence of bytes is to use std::bit_cast. Since the range may be large,
			// we don't want to copy it to a local buffer as it could cause stack overflow,
			// so instead we allocate a buffer on the heap and copy the elements one by one
			// using std::bit_cast
			// Note: since C++20 if an allocation is freed in the same expression it was allocated
			// in it is allowed to be a constant expression The buffer is then passed to the hash
			// algorithm. The memory is freed in the same scope and compiler should also see that
			// the buffer is a memcopy of the range's data. This should allow for copy elision and
			// no overhead.

			constexpr std::size_t ELEM_SIZE   = sizeof(std::ranges::range_value_t<R>);
			const std::size_t     r_size      = std::ranges::size(r);
			const std::size_t     buffer_size = r_size * ELEM_SIZE;
			auto* const           buffer      = ::new std::byte[buffer_size];

			for (u64 i = 0, j = 0; i < r_size; ++i, j += ELEM_SIZE) {
				// Ranges may have both singed and unsigned index types and there is not good trait
				// that can always tell which one the range expects. To suppress warnings we get the
				// elements using std::next with range's difference_type
				const auto& elem = *std::next(
					std::ranges::begin(r), static_cast<std::ranges::range_difference_t<R>>(i)
				);
				const auto arr = std::bit_cast<std::array<const std::byte, ELEM_SIZE>>(elem);
				std::copy(arr.begin(), arr.end(), buffer + j);
			}

			h(std::span<const std::byte>{ buffer, buffer_size });

			delete[] buffer;
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
