#pragma once

#include <type_traits>
#include <concepts>

#include <base/type_traits.hpp>

#include "hashing_algorithms.hpp"
#include "add_to_hash.hpp"
#include "type_hash_code.hpp"
#include "type_unique_code.hpp"
#include "type_code.hpp"

namespace hashing {


	namespace detail {

		/**
		 * Checks if a given type is a type code or a void type
		 */
		template<typename T>
		concept TypeCode_or_void
			= std::is_void_v<T> || base::IsInstantiationOfTypeValue<T, TypeCode>;

		/**
		 * Checks if a given type has a nested value_type member type
		 */
		template<class T>
		concept has_value_type = requires { typename T::value_type; };

		/**
		 * Helper struct that enables the TypeCode_value_type member type
		 * if the given type has a value_type member type
		 * (this is a default implementation that does not add the member type)
		 */
		template<class TypeC>
		struct enable_TypeHC_value_type {};

		/**
		 * Specialization that adds TypeCode_value_type
		 */
		template<has_value_type TypeC>
		struct enable_TypeHC_value_type<TypeC> {
			using TypeCode_value_type = TypeC::value_type;
		};

		/**
		 * Checks if a given type hash code can be hashed by the given hash algorithm
		 * (it also protects from invalid instantiations outside of the immediate context
		 * that could cause a compilation error by omitting SFINAE,
		 * i.e. checks all required steps for the instantiation to be valid one by one
		 * and returns false on the first invalid step instead of failing to compile)
		 */
		template<typename T, typename TypeC, typename HashAlgorithm>
		concept can_get_type_hash_code = requires(HashAlgorithm& h) {
			typename TypeC::value_type;
			uniqueString<T, typename TypeC::value_type, HashAlgorithm>();
			static_cast<TypeCode<typename TypeC::value_type>>(
				std::declval<StrToIntegral<typename TypeC::value_type, HashAlgorithm>>()
			);
			detail::getIDFromUniqueString<T, typename TypeC::value_type, HashAlgorithm>();
			addToHash(h, TYPE_HASH_CODE<T, typename TypeC::value_type, HashAlgorithm>);
		};

		/**
		 * Adds to hash either a TYPE_UNIQUE_CODE or a TYPE_HASH_CODE
		 * depending on the value of IS_UNIQUE constant in the provided TypeC
		 */
		template<
			base::IsInstantiationOfTypeValue<TypeCode> TypeC,
			typename HashAlgorithm,
			typename T>
		constexpr void addTypeCode(HashAlgorithm& h) noexcept {
			if (TypeC::IS_UNIQUE)
				addToHash(h, TYPE_UNIQUE_CODE<T, typename TypeC::value_type>);
			else if constexpr (detail::can_get_type_hash_code<T, TypeC, HashAlgorithm>)
				addToHash(h, TYPE_HASH_CODE<T, typename TypeC::value_type, HashAlgorithm>);
			else if constexpr (detail::can_get_type_hash_code<T, TypeC, DefaultHashAlgorithm>)
				addToHash(h, TYPE_HASH_CODE<T, typename TypeC::value_type, DefaultHashAlgorithm>);
			else
				addToHash(h, TYPE_HASH_CODE<T>);
		}

	}  // namespace detail

	/**
	 * @brief Callable type that obtains a hash for an object it is called with
	 * together with it's type code using the specified hash algorithm
	 *
	 * @tparam HashAlgorithm - Hashing algorithm to use
	 * @tparam TypeC - Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode<>>
	class Hash final: public detail::enable_TypeHC_value_type<TypeC> {
	public:
		static constexpr bool APPEND_TYPE_CODE = not std::is_void_v<TypeC>;
		using result_type                      = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h{};

			addToHash(h, t);
			if constexpr (APPEND_TYPE_CODE) detail::addTypeCode<TypeC, HashAlgorithm, T>(h);

			return h.finalize();
		}
	};

	/**
	 * @brief Callable type that obtains hash values for sequences of objects it is called with
	 * Type keeps the state between calls, so next objects can be appended.
	 * Calling finalize() yields the hash value corresponding to the current state
	 *
	 * @tparam HashAlgorithm - Hashing algorithm to use
	 * @tparam TypeC - Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode<>>
	class StatefulHash final: public detail::enable_TypeHC_value_type<TypeC> {
		HashAlgorithm h{};

	public:
		static constexpr bool APPEND_TYPE_CODE = not std::is_void_v<TypeC>;
		using result_type                      = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr StatefulHash& operator()(const T& t) noexcept {
			addToHash(h, t);
			if constexpr (APPEND_TYPE_CODE) detail::addTypeCode<TypeC, HashAlgorithm, T>(h);

			return *this;
		}

		template<typename... Ts>
		constexpr StatefulHash& operator()(const Ts&... ts) noexcept {
			return ((this->operator()(ts)), ...);
		}

		[[nodiscard]]
		constexpr result_type finalize() noexcept {
			return h.finalize();
		}
	};

	/**
	 * @brief Gets the hash value for the object using the specified hash algorithm
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam TypeC - type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 * @param t - object to hash
	 * @return hash value
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode<>>
	auto justHash(const auto& t) {
		return Hash<HashAlgorithm, TypeC>{}(t);
	}

	/**
	 * @brief Variadic version of justHash(), uses StatefulHash to hash multiple objects
	 *
	 * @tparam HashAlgorithm - type of the hashing algorithm to use
	 * @tparam TypeC - type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 * @param ts - objects to hash
	 * @return hash value
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode<>>
	auto justHash(const auto&... ts) {
		return StatefulHash<HashAlgorithm, TypeC>{}(ts...).finalize();
	}


}  // namespace hashing
