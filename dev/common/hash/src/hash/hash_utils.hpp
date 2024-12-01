#pragma once
#include <concepts>

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

	template<typename T>
	concept has_update_hash_void
		= requires(T t, void* data, usize len) { t.update_hash(data, len); };
	template<typename T>
	concept has_update_hash_char
		= requires(T t, char* data, usize len) { t.update_hash(data, len); };

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

		template<has_update_hash_void Self, typename T>
		requires(std::has_unique_object_representations_v<T> && !std::ranges::input_range<T> && !has_update_hash_char<Self>)
		/*constexpr*/ auto&& operator()(this Self&& self, const T& t) noexcept {
			self.update_hash(std::addressof(t), sizeof(t));
			return std::forward<decltype(self)>(self);
		}

		template<has_update_hash_char Self, typename T>
		requires(std::has_unique_object_representations_v<T> && !std::ranges::input_range<T>)
		constexpr auto&& operator()(this Self&& self, const T& t) noexcept {
			std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
			std::forward<Self>(self).update_hash(arr.data(), arr.size());
			return std::forward<Self>(self);
		}

		template<std::ranges::input_range R>
		requires std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		/*constexpr*/ auto&& operator()(this has_update_hash_void auto&& self, R&& range) noexcept(
			noexcept(std::ranges::data(range), std::ranges::size(range))
		) {
			if constexpr (std::ranges::contiguous_range<R>) {
				self.update_hash(
					std::ranges::data(range),
					std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>)
				);
			} else {
				for (auto&& elem: range) self.update_hash(std::addressof(elem), sizeof(elem));
			}
			return std::forward<decltype(self)>(self);
		}
	};


}  // namespace hashing
