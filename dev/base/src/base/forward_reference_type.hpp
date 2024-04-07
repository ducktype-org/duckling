#pragma once

#include <utility>

namespace base {
	template<typename T>
	auto forwardReferenceMaker(T&& arg) -> decltype(std::forward<T>(arg)) {
		return std::forward<T>(arg);
	}
}

#define FORWARD_TYPE(expr) decltype(base::forwardReferenceMaker(expr))

// @TODO: see what uses can be changed to decltype(auto)
// beware of difference between "(a)" and "a".
// see example in: https://en.cppreference.com/w/cpp/language/auto
#define DECL_FORWARDING_VAR(var, expr) FORWARD_TYPE(expr) var = (expr)
