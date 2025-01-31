#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
#include <ranges>
#include <array>
#include <bit>


namespace hashing {


	template<typename T>
	concept hash_algorithm = requires {
		std::is_object_v<T>;
		std::is_constructible_v<T>;
		std::is_destructible_v<T>;

		std::is_invocable_v<T, void*, usize> || std::is_invocable_v<T, char*, usize>;
		typename T::result_type;
		std::is_convertible_v<T, typename T::result_type>;
	};

	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void addToHash(HashAlgorithm& h, const T& t);

	namespace detail {

		template<typename HashAlgorithm, typename T>
		concept can_hash_directly
			= hash_algorithm<HashAlgorithm> && requires(HashAlgorithm& h, const T& t) { h(t); };

		using std::hash;
		template<typename T>
		concept can_stdhash = requires(const T& t) { hash<T>{}(t); };

		template<typename T, template<typename...> typename Templ>
		concept specialization_of = requires(T t) {
			[]<typename... Args>(Templ<Args...>)
				requires std::is_same_v<Templ<Args...>, T>
			{}(t);
		};

		template<typename T>
		concept tuple_of_refs = requires(T t) {
			[]<typename... Args>(std::tuple<Args...>)
				requires std::is_same_v<std::tuple<Args...>, T> && (std::is_reference_v<Args> && ...)
			{}(t);
		};

		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> tuple_of_refs;
		};

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsChars(HashAlgorithm& h, const T& t) {
			std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
			h(arr.data(), arr.size());
		}

		template<typename HashAlgorithm, typename R>
		concept is_range_with_hashable_elements
			= hash_algorithm<HashAlgorithm> && std::ranges::input_range<R>
		   && requires(HashAlgorithm& h, const R& t) {
				  addToHash(h, std::declval<std::ranges::range_value_t<R>>());
			  };

		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_chars
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		   && requires(HashAlgorithm& h, const R& t) {
				  h(std::ranges::data(t),
			        std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
			  };

		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		constexpr void hashRangeAsChars(HashAlgorithm& h, const R& t)
			requires can_hash_range_as_chars<HashAlgorithm, R> {
			h(std::ranges::data(t), std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
		}
	}


}  // namespace hashing
