#pragma once

#include <type_traits>
#include <string_view>

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

	// Thanks to https://stackoverflow.com/a/56766138
	template<class T>
	constexpr auto type_name() {
		std::string_view name, prefix, suffix;
#ifdef __clang__
		name   = __PRETTY_FUNCTION__;
		prefix = "auto base::type_name() [T = ";
		suffix = "]";
#elif defined(__GNUC__)
		name   = __PRETTY_FUNCTION__;
		prefix = "constexpr auto base::type_name() [with T = ";
		suffix = "]";
#elif defined(_MSC_VER)
		name   = __FUNCSIG__;
		prefix = "auto __cdecl base::type_name<";
		suffix = ">(void)";
#endif
		name.remove_prefix(prefix.size());
		name.remove_suffix(suffix.size());
		return name;
	}
}
