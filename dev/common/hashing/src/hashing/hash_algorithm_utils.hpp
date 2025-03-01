#pragma once

#include <type_traits>
#include <concepts>
#include <cstdint>
#include <utility>
#include <ranges>
#include <array>
#include <tuple>
#include <span>
#include <bit>

#include <base/ints.hpp>

namespace hashing {


	namespace detail {

		/**
		 * checks if the type can be invoked with a span of byte
		 */
		template<typename T>
		concept invocable_with_byte_span
			= requires(T t) { t(std::declval<std::span<std::byte>>()); };

		/**
		 * checks if the type has a finalize() method that returns a result_type
		 */
		template<typename T>
		concept has_finalize = requires(T t) {
			{ t.finalize() } -> std::same_as<typename T::result_type>;
		};

		/**
		 * implementation of the hash_algorithm concept
		 */
		template<typename T>
		concept hash_algorithm_impl
			= std::is_object_v<T> && std::is_constructible_v<T> && std::is_destructible_v<T>
		   && requires { typename T::result_type; }
		   && invocable_with_byte_span<T> && has_finalize<T>;

	}  // namespace detail

	/**
	 * checks if the type is a hash algorithm
	 */
	template<typename T>
	concept hash_algorithm = detail::hash_algorithm_impl<std::remove_cvref_t<T>>;

	/**
	 * returns a reference to one of the Bases of Derived
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

	namespace detail {

		/**
		 * checks if the hash algorithm can hash the type directly (i.e. it has an operator() that
		 * accepts the type)
		 */
		template<typename HashAlgorithm, typename T>
		concept can_hash_directly
			= hash_algorithm<HashAlgorithm> && requires(HashAlgorithm& h, const T& t) { h(t); };

		/**
		 * checks if the type can be hashed with std::hash
		 */
		template<typename T>
		concept can_stdhash = requires(const T& t) { std::hash<T>{}(t); };

		/**
		 * checks if the type is a tuple of references
		 */
		template<typename T>
		concept tuple_of_refs = requires(T t) {
			[]<typename... Args>(std::tuple<Args...>)
				requires std::is_same_v<std::tuple<Args...>, T>
			          && (std::is_reference_v<Args> && ...) {}(t);
		};

		/**
		 * checks if hashDecompose() can be called on the type and if it returns a tuple of
		 * references
		 */
		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> tuple_of_refs;
		};

		/**
		 * hashes an object as a sequence of bytes
		 */
		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsChars(HashAlgorithm&& h, const T& t) {
			const auto arr  = std::bit_cast<std::array<std::byte, sizeof(T)>, T>(t);
			const auto span = std::span{ arr.data(), arr.size() };
			std::forward<HashAlgorithm>(h)(span);
		}

		/**
		 * checks if a range can be hashed as a contiguous sequence of memory
		 * (i.e. its elements are in a contiguous memory block, have unique object representations
		 * and size is known)
		 */
		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_chars
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		   && requires(HashAlgorithm& h, const R& t) {
				  std::span{ std::ranges::data(t), std::ranges::size(t) };
				  h(std::as_bytes(std::span{ std::ranges::data(t), std::ranges::size(t) }));
			  };

		/**
		 * hashes a range as a contiguous sequence of memory
		 */
		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		requires can_hash_range_as_chars<HashAlgorithm, R>
		constexpr void hashRangeAsChars(HashAlgorithm& h, const R& t) {
			const auto span = std::span{ std::ranges::data(t), std::ranges::size(t) };
			h(std::as_bytes(span));
		}

		/**
		 * checks if a range that may have unspecified order of elements can be hashed
		 */
		template<typename HashAlgorithm, typename R>
		concept can_hash_range_with_unspecified_order
			= std::copy_constructible<HashAlgorithm> && std::ranges::input_range<R>
		   && requires(HashAlgorithm::result_type res) {
				  {
					  res ^= res
				  }
				  -> std::convertible_to<std::remove_cvref_t<typename HashAlgorithm::result_type>>;
			  };

		/**
		 * checks if the type is tuple-like i.e. supports std::tuple_size and std::get
		 */
		template<typename T>
		concept supports_std_get = requires {
			// don't remove this line and don't change to std::tuple_size_v
			// if std::tuple_size_v is ill-formed, the fail may happen not in the immediate context
			// of the concept check which may omit SFINAE
			std::tuple_size<T>::value;

			[]<std::size_t... Is>(std::index_sequence<Is...>) requires requires {
				(std::get<Is>(std::declval<T>()), ...);
			} {}(std::make_index_sequence<
				 /* don't change to std::tuple_size_v */ std::tuple_size<T>::value>{});
		};

	}  // namespace detail


}  // namespace hashing
