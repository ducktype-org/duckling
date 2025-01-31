#pragma once
#include <concepts>

#include "type_hash_code_def.hpp"


namespace hashing {


	namespace detail {

		// updateHash should be protected
		template<typename Self, typename CheckT>
		struct CheckAccessTo_updateHash: public std::remove_cvref_t<Self> {
			void check(CheckT* data, usize len)
				requires requires { CheckAccessTo_updateHash{}.std::remove_cvref_t<Self>::updateHash(data, len); } {}
		};

		// addHashCode should be protected
		template<typename Self>
		struct CheckAccessTo_addHashCode: public std::remove_cvref_t<Self> {
			void check(const TypeHashCode& hash)
				requires requires { CheckAccessTo_addHashCode{}.std::remove_cvref_t<Self>::addHashCode(hash); } {}
		};

	} // namespace detail

	template<typename T>
	concept has_updateHash_void
		= requires(T t, void* data, usize len) { detail::CheckAccessTo_updateHash<T, void>{}.check(data, len); };

	template<typename T>
	concept has_updateHash_char
		= requires(T t, char* data, usize len) { detail::CheckAccessTo_updateHash<T, char>{}.check(data, len); };

	template<typename T>
	concept has_updateHash = has_updateHash_void<T> || has_updateHash_char<T>;

	template<typename Self, typename T>
	constexpr bool IS_HASH_CODE_AWARE
		= std::is_same_v<std::remove_cvref_t<T>, TypeHashCode>
	   && requires(const TypeHashCode& hash) { detail::CheckAccessTo_addHashCode<Self>{}.check(hash); };

	class CallOverloads {
	public:
		template<has_updateHash_void Self>
		auto&& operator()(this Self&& self, const void* data, usize len) noexcept {
			std::forward<Self>(self).updateHash(data, len);
			return std::forward<Self>(self);
		}

		template<has_updateHash_char Self>
		constexpr auto&& operator()(this Self&& self, const char* data, usize len) noexcept {
			std::forward<Self>(self).updateHash(data, len);
			return std::forward<Self>(self);
		}

		template<has_updateHash Self, typename T>
		requires(std::has_unique_object_representations_v<T>)
		constexpr auto&& operator()(this Self&& self, const T& t) noexcept {
			if constexpr (IS_HASH_CODE_AWARE<Self, T>) {
				std::forward<Self>(self).addHashCode(t);
			} else if constexpr (has_updateHash_char<Self>) {
				std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
				std::forward<Self>(self).updateHash(arr.data(), arr.size());
			} else {
				std::forward<Self>(self).updateHash(std::addressof(t), sizeof(t));
			}
			return std::forward<Self>(self);
		}
	};


}  // namespace hashing
