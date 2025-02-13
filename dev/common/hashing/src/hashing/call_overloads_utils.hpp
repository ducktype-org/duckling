#pragma once

#include <type_traits>
#include <string_view>
#include <concepts>
#include <utility>
#include <memory>
#include <array>
#include <bit>


#include "type_hash_code_def.hpp"

namespace hashing {


	namespace detail {

		/**
		 * utility class for testing if updateHash is protected
		 */
		template<typename Self, typename CheckT>
		struct CheckAccessTo_updateHash_ptr: public std::remove_cvref_t<Self> {
			// should be callable from derived class but not from outside
			void check(CheckT* data, usize len) requires requires {
				CheckAccessTo_updateHash_ptr<Self, CheckT>{}.std::remove_cvref_t<Self>::updateHash(
					data, len
				);
			} && (not requires(Self self) { self.updateHash(data, len); }) {}
		};

		/**
		 * utility class for testing if updateHash is protected
		 */
		template<typename Self>
		struct CheckAccessTo_updateHash_sv: public std::remove_cvref_t<Self> {
			// should be callable from derived class but not from outside
			void check(const std::string_view sv) requires requires {
				CheckAccessTo_updateHash_sv<Self>{}.std::remove_cvref_t<Self>::updateHash(sv);
			} && (not requires(Self self) { self.updateHash(sv); }) {}
		};

		/**
		 *  utility class for testing if addHashCode is protected
		 */
		template<typename Self>
		struct CheckAccessTo_addHashCode: public std::remove_cvref_t<Self> {
			// should be callable from derived class but not from outside
			template<base::IsInstantiationOf<TypeHashCodeBase> TypeHC>
			void check(TypeHC hash) requires requires {
				CheckAccessTo_addHashCode{}.std::remove_cvref_t<Self>::addHashCode(hash);
			} && (not requires(Self self) { self.addHashCode(hash); }) {}
		};

	}  // namespace detail

	/**
	 * chechs if the type has protected updateHash(void*, usize) function
	 */
	template<typename T>
	concept has_updateHash_void = requires(T t, void* data, usize len) {
		detail::CheckAccessTo_updateHash_ptr<T, void>{}.check(data, len);
	};

	/**
	 * chechs if the type has protected updateHash(std::string_view) function
	 */
	template<typename T>
	concept has_updateHash_sv = requires(T t, std::string_view sv) {
		detail::CheckAccessTo_updateHash_sv<T>{}.check(sv);
	};

	template<typename T>
	concept has_updateHash = has_updateHash_void<T> || has_updateHash_sv<T>;

	/**
	 * checks if the type has protected addHashCode function for the given hash code type
	 */
	template<typename T, typename TypeHC>
	concept has_addHashCode
		= base::IsInstantiationOf<TypeHC, TypeHashCodeBase> && requires(T t, TypeHC hash_code) {
			  detail::CheckAccessTo_addHashCode<T>{}.check(hash_code);
		  };

	/**
	 * checks if the type should be hashed as a hash code
	 */
	template<typename HashAlgorithm, typename Type>
	static constexpr bool SHOULD_HASH_AS_HASH_CODE
		= base::IsInstantiationOf<Type, TypeHashCodeBase> && has_addHashCode<HashAlgorithm, Type>;

	/**
	 * this is a utility class that adds operator() overloads to hashing algorithms
	 * if appropriate updateHash() functions are provided in the derived class
	 */
	class CallOverloads {
	public:
		// note that this one is not actually constexpr as updateHash() can't be in c++23 (it can in
		// c++26)
		template<has_updateHash_void Self>
		constexpr decltype(auto
		) operator()(this Self&& self, const void* data, const usize len) noexcept {
			std::forward<Self>(self).updateHash(data, len);
			return std::forward<Self>(self);
		}

		template<has_updateHash_sv Self>
		constexpr decltype(auto) operator()(this Self&& self, const std::string_view sv) noexcept {
			std::forward<Self>(self).updateHash(sv);
			return std::forward<Self>(self);
		}

		// enables constexpr hashing of types with unique object representations
		template<has_updateHash Self, typename T>
		requires(
			std::has_unique_object_representations_v<T>
			&& not(has_updateHash_sv<Self> && std::convertible_to<T, std::string_view> && not std::same_as<std::remove_cvref_t<T>, char*>)
		)
		constexpr decltype(auto) operator()(this Self&& self, const T& t) noexcept {
			if constexpr (SHOULD_HASH_AS_HASH_CODE<Self, T>) {
				std::forward<Self>(self).addHashCode(t);
			} else if constexpr (has_updateHash_sv<Self>) {
				std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
				std::forward<Self>(self).updateHash(std::string_view{ arr.data(), arr.size() });
			} else {
				std::forward<Self>(self).updateHash(std::addressof(t), sizeof(t));
			}
			return std::forward<Self>(self);
		}
	};


}  // namespace hashing
