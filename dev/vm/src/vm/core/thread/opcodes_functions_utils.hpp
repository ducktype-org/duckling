#pragma once

#include <base/ints.hpp>

template<typename T>
[[gnu::always_inline]]
inline static T& derefStack(std::byte* stack, i64 position) {
	return *(reinterpret_cast<T*>(&stack[position]));
}
