#pragma once
#include <concepts>

#include <base/ints.hpp>

#include "unique_id.hpp"

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

	namespace detail {
		template<typename Self, typename CheckT>
		struct acc: public std::remove_cvref_t<Self> {
			void check(CheckT* data, usize len)
				requires requires { acc{}.std::remove_cvref_t<Self>::update_hash(data, len); } {}
		};

		template<typename Self>
		struct hc_acc: public std::remove_cvref_t<Self> {
			void check(const type_hash_code_t& hash)
				requires requires { hc_acc{}.std::remove_cvref_t<Self>::add_hash_code(hash); } {}
		};
	}

	template<typename T>
	concept has_update_hash_void
		= requires(T t, void* data, usize len) { detail::acc<T, void>{}.check(data, len); };

	template<typename T>
	concept has_update_hash_char
		= requires(T t, char* data, usize len) { detail::acc<T, char>{}.check(data, len); };

	template<typename T>
	concept has_update_hash = has_update_hash_void<T> || has_update_hash_char<T>;

	template<typename Self, typename T>
	constexpr bool is_hash_code_aware
		= std::is_same_v<std::remove_cvref_t<T>, type_hash_code_t>
	   && requires(const type_hash_code_t& hash) { detail::hc_acc<Self>{}.check(hash); };

	class call_overloads {
	public:
		template<has_update_hash_void Self>
		/*constexpr*/ auto&& operator()(this Self&& self, const void* data, usize len) noexcept {
			std::forward<Self>(self).update_hash(data, len);
			return std::forward<Self>(self);
		}

		template<has_update_hash_char Self>
		constexpr auto&& operator()(this Self&& self, const char* data, usize len) noexcept {
			std::forward<Self>(self).update_hash(data, len);
			return std::forward<Self>(self);
		}

		template<has_update_hash Self, typename T>
		requires(std::has_unique_object_representations_v<T>)
		constexpr auto&& operator()(this Self&& self, const T& t) noexcept {
			if constexpr (is_hash_code_aware<Self, T>) {
				std::forward<Self>(self).add_hash_code(t);
			} else if constexpr (has_update_hash_char<Self>) {
				std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
				std::forward<Self>(self).update_hash(arr.data(), arr.size());
			} else {
				std::forward<Self>(self).update_hash(std::addressof(t), sizeof(t));
			}
			return std::forward<Self>(self);
		}
	};

	namespace detail {
		template<typename HashAlgorithm, typename T>
		concept can_add_to_hash = hash_algorithm<HashAlgorithm>
		                       && requires(HashAlgorithm& h, const T& t) { add_to_hash(h, t); };

		template<typename HashAlgorithm, typename T>
		concept can_hash_directly
			= hash_algorithm<HashAlgorithm> && requires(HashAlgorithm& h, const T& t) { h(t); };

		using std::hash;
		template<typename T>
		concept can_stdhash = requires(const T& t) { hash<T>{}(t); };

		template<typename T>
		concept can_hash_decompose = requires(const T& t) { hash_decompose(t); };

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hash_as_chars(HashAlgorithm& h, const T& t) {
			std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
			h(arr.data(), arr.size());
		}

		template<typename HashAlgorithm, typename R>
		concept is_range_with_hashable_elements
			= hash_algorithm<HashAlgorithm> && std::ranges::input_range<R>
			&& requires(HashAlgorithm& h, const R& t) {
				add_to_hash(h, std::declval<std::ranges::range_value_t<R>());
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
		constexpr void hash_range_as_chars(HashAlgorithm& h, const R& t)
			requires can_hash_range_as_chars<HashAlgorithm, R> {
			h(std::ranges::data(t), std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
		}

		

	}

}  // namespace hashing
