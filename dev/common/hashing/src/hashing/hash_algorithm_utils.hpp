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

	using byte_like_types_tuple = std::
		tuple<std::byte, unsigned char, std::uint8_t, char8_t, char, signed char, std::int8_t>;

	static constexpr std::tuple BYTE_LIKE_TYPES_TUPLE_V = byte_like_types_tuple{};

	template<typename T>
	static constexpr bool BYTE_LIKE_TYPE = []<typename... Ts>(std::tuple<Ts...>) {
		return (std::is_same_v<std::remove_const_t<T>, Ts> || ...);
	}(BYTE_LIKE_TYPES_TUPLE_V);


	template<typename T>
	concept byte_like = BYTE_LIKE_TYPE<T>;

	/**
	 * checks if the type can be invoked with a span of type B
	 */
	template<typename T, typename B>
	concept invocable_with_span = requires(T t) { t(std::span<B>{}); };

	/**
	 * checks if the type can be invoked with some span of byte-like type
	 */
	template<typename T>
	concept invocable_with_byte_span
		= invocable_with_span<T, std::byte> || invocable_with_span<T, unsigned char>
	   || invocable_with_span<T, std::uint8_t> || invocable_with_span<T, char8_t>
	   || invocable_with_span<T, char> || invocable_with_span<T, signed char>
	   || invocable_with_span<T, std::int8_t>;
	// []<typename... Ts>(std::tuple<Ts...>) {
	// 	return (invocable_with_span<T, Ts> || ...);
	// }(BYTE_LIKE_TYPES_TUPLE_V);

	/**
	 * finds index of the first span of bytes that the type accepts
	 */
	template<typename Algorithm>
	static constexpr std::size_t FIRST_MATCHING_SPAN_INDEX = []<typename... Ts>(std::tuple<Ts...>) {
		static_assert(invocable_with_byte_span<Algorithm>);
		constexpr std::array arr = { invocable_with_span<Algorithm, Ts>... };
		for (std::size_t i = 0; i < arr.size(); ++i)
			if (arr[i]) return i;
		// unreachable
		return arr.size();
	}(BYTE_LIKE_TYPES_TUPLE_V);

	/**
	 * finds the first byte-like type, span of which the type can accept
	 */
	template<typename Algorithm>
	using first_matching_byte_like_t
		= std::tuple_element_t<FIRST_MATCHING_SPAN_INDEX<Algorithm>, byte_like_types_tuple>;

	template<typename Algorithm>
	using first_matching_span = std::span<first_matching_byte_like_t<Algorithm>>;

	namespace detail {

		template<typename T>
		concept has_finalize = requires(T t) {
			{ t.finalize() } -> std::same_as<typename T::result_type>;
		};

		template<typename T>
		concept hash_algorithm_impl
			= std::is_object_v<T> && std::is_constructible_v<T> && std::is_destructible_v<T>
		   && requires { typename T::result_type; }
		   && invocable_with_byte_span<T> && has_finalize<T>;

	}  // namespace detail

	template<typename T>
	concept hash_algorithm = detail::hash_algorithm_impl<std::remove_cvref_t<T>>;

	/**
	 * returns a reference to one of the Bases of Derived
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

		template<typename T>
		concept can_stdhash = requires(const T& t) { std::hash<T>{}(t); };

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
			using byte_t    = first_matching_byte_like_t<HashAlgorithm>;
			const auto arr  = std::bit_cast<std::array<byte_t, sizeof(T)>, T>(t);
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
			} {}(std::make_index_sequence<std::tuple_size<T>::value>{}
			);  // don't change to std::tuple_size_v
		};

	}  // namespace detail


}  // namespace hashing
