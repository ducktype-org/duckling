#pragma once

#include <type_traits>

namespace base {
	namespace detail {
		template<template<typename...> class Template, typename>
		struct IsInstantiationOfImpl: std::false_type {};

		template<template<typename...> class Template, typename... Args>
		struct IsInstantiationOfImpl<Template, Template<Args...>>: std::true_type {};

		template<class, template<class> class>
		struct IsOfSameClassImpl: public std::false_type {};

		template<class T2, template<class> class U>
		struct IsOfSameClassImpl<U<T2>, U>: public std::true_type {};
	}

	template<template<typename...> class Template, typename T>
	concept IsInstantiationOf = detail::IsInstantiationOfImpl<Template, T>::value;

	/**
	 * This concept is used to statically determine if two types are instances of the same templated
	 * class.
	 *
	 * Example:
	 * template<class T>
	 * class A { ... };
	 *
	 * template<class T>
	 * class B { ... };
	 *
	 * static_assert(IsOfSameClass<A<int>, A<bool>>); // passes
	 * static_assert(IsOfSameClass<A<int>, B<int>>);  // fails
	 *
	 * @tparam TypeA
	 * @tparam TypeB
	 */
	template<class TypeA, template<class> class TypeB>
	concept IsOfSameClass = detail::IsOfSameClassImpl<TypeA, TypeB>::value;
}
