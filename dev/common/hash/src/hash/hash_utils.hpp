#pragma once
#include <concepts>

#include "unique_id.hpp"

namespace hashing {


	template<typename T>
	concept hash_algorithm = requires {
		std::is_object_v<T>;
		std::is_constructible_v<T>;
		std::is_destructible_v<T>;

		std::is_invocable_v<T, void*, usize> || std::is_invocable_v<T, char*, usize>;
		typename T::result_type;
		std::is_convertible_v<const T, typename T::result_type>;
	};

	namespace detail {
		template<typename Self, typename CheckT>
		struct acc : public std::remove_cvref_t<Self> {
			void check(CheckT* data, usize len) requires requires {
				std::remove_cvref_t<Self>::update_hash(data, len);
			}{}
		};
	}

	template<typename T>
	concept has_update_hash_void = requires(T t, void* data, usize len) {
		[](void* data, usize len) requires requires {
			detail::acc<T, void>{}.check(data, len);
		}{}(data, len);
	};

	template<typename T>
	concept has_update_hash_char = requires(T t, char* data, usize len) {
		[](char* data, usize len) requires requires {
			detail::acc<T, char>{}.check(data, len);
		}{}(data, len);
	};

	template<typename T>
	concept has_update_hash = has_update_hash_void<T> || has_update_hash_char<T>;

	namespace detail {
	template<typename Self>
	struct hc_acc: public std::remove_cvref_t<Self> {
		void check(const type_hash_code_t& hash) requires requires {
			std::remove_cvref_t<Self>::add_hash_code(hash);
		}{}
	};
	}
	
	template<typename Self, typename T>
	constexpr bool is_hash_code_aware
		= std::is_same_v<std::remove_cvref_t<T>, type_hash_code_t>
		&& requires(const type_hash_code_t& hash) {
			detail::hc_acc<Self>{}.check(hash);
		};
		

	class call_overloads {
	public:
		template<has_update_hash_void Self>
		/*constexpr*/ auto&&
			operator()(this Self&& self, const volatile void* data, usize len) noexcept {
			std::forward<Self>(self).update_hash(const_cast<void*>(data), len);
			return std::forward<Self>(self);
		}

		template<has_update_hash_char Self>
		constexpr auto&& operator()(this Self&& self, const char* data, usize len) noexcept {
			std::forward<Self>(self).update_hash(data, len);
			return std::forward<Self>(self);
		}

		template<has_update_hash Self, typename T>
		requires(std::has_unique_object_representations_v<T> && !std::ranges::input_range<T>)
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

		template<has_update_hash_void Self, std::ranges::input_range R>
		requires std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		constexpr auto&& operator()(this Self&& self, R&& range) noexcept(
			noexcept(std::ranges::data(range), std::ranges::size(range))
		) {
			if constexpr (std::ranges::contiguous_range<R>) {
				std::forward<Self>(self).update_hash(
					std::ranges::data(range),
					std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>)
				);
			} else {
				for (auto&& elem: range)
					std::forward<Self>(self).update_hash(std::addressof(elem), sizeof(elem));
			}
			return std::forward<Self>(self);
		}
	};


}  // namespace hashing
