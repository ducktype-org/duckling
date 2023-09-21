#pragma once

#include <type_traits>

namespace base {
	namespace detail {
		template<template<typename...> class Template, typename>
		struct IsInstantiationOfImpl: std::false_type {};

		template<template<typename...> class Template, typename... Args>
		struct IsInstantiationOfImpl<Template, Template<Args...>>: std::true_type {};
	}

	template<template<typename...> class Template, typename T>
	concept IsInstantiationOf = detail::IsInstantiationOfImpl<Template, T>::value;
}
