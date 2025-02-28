/**
 * @file type_traits.hpp
 *
 * @brief Type traits provides a set of functionalities useful during metaprogramming with types.
 *
 * Functionalities
 * ===============
 *
 * Concepts:
 * ---------
 * - base::IsInstantiationOf
 * - base::IsNumber
 * - base::IsOfSameClass
 *
 * Functions:
 * ----------
 * - base::typeName
 *
 * Variables:
 * ----------
 * - base::Implication
 *
 * ### Usage
 * @include type_traits_example.cpp
 *
 * @example type_traits_example.cpp
 */
#pragma once

#include <type_traits>
#include <string_view>

namespace base {
	namespace detail {
		template<class, class>
		struct IsOfSameClassImpl: public std::false_type {};

		template<class T1, class T2, template<class> class U>
		struct IsOfSameClassImpl<U<T1>, U<T2>>: public std::true_type {};
	}

	/**
	 * @brief Checks if type `T` is an instantiation of template `Template`.
	 * @note This concept works only for templates that have only type template parameters
	 */
	template<typename T, template<typename...> typename Template>
	concept IsInstantiationOf = requires(T t) {
		[]<typename... Args>(Template<Args...>) requires std::is_same_v<Template<Args...>, T> {}(t);
	};

	/**
	 * @brief Checks if type `T` is an instantiation of template `Template`.
	 * @note This concept works only for templates that take one type and one value template
	 * parameter
	 */
	template<typename T, template<typename, auto> class Template>
	concept IsInstantiationOfTypeValue = requires(T t) {
		[]<typename U, auto V>(Template<U, V>) requires std::is_same_v<Template<U, V>, T> {}(t);
	};

	/**
	 * @brief Checks if type `T` is an integral or floating point number.
	 */
	template<typename T>
	concept IsNumber = std::is_floating_point_v<T> || std::is_integral_v<T>;


	/**
	 * This concept is used to statically determine if two types are instances of the same templated
	 * class.
	 *
	 * Example:
	 * @n template<class T>
	 * @n class A { ... };
	 *
	 * template<class T>
	 * @n class B { ... };
	 *
	 * static_assert(IsOfSameClass<A<int>, A<bool>>); // passes
	 * @n static_assert(IsOfSameClass<A<int>, B<int>>);  // fails
	 */
	template<class TypeA, class TypeB>
	concept IsOfSameClass = detail::IsOfSameClassImpl<TypeA, TypeB>::value;

	/**
	 * @brief Checks if A implies B.
	 */
	template<bool A, bool B>
	constexpr bool Implication = !A || B;

	/**
	 * @brief Returns the name of the passed type `T`.
	 *
	 * @note From https://stackoverflow.com/a/56766138
	 */
	template<class T, bool pretty = true>
	constexpr auto typeName() {
		std::string_view name, prefix, suffix;
#ifdef __clang__
		name   = __PRETTY_FUNCTION__;
		prefix = "auto base::typeName() [T = ";
		suffix = ", pretty = true]";
#elif defined(__GNUC__)
		name   = __PRETTY_FUNCTION__;
		prefix = "constexpr auto base::typeName() [with T = ";
		suffix = "; bool pretty = true]";
#elif defined(_MSC_VER)
		name   = __FUNCSIG__;
		prefix = "auto __cdecl base::type_name<";
		suffix = ",true>(void)";
#endif
		if constexpr (pretty) {
			name.remove_prefix(prefix.size());
			name.remove_suffix(suffix.size());
		}
		return name;
	}
}
