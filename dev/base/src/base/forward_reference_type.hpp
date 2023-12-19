#pragma once

#include <utility>

namespace base {
	template<typename T>
	auto forwardReferenceMaker(T&& arg) -> decltype(std::forward<T>(arg)) {
		return std::forward<T>(arg);
	}
}

#define FORWARD_TYPE(expr) decltype(base::forwardReferenceMaker(expr))

#define DECL_FORWARDING_VAR(var, expr) FORWARD_TYPE(expr) var = (expr)
