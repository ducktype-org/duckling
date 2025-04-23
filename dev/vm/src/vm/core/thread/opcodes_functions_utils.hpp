#pragma once

#include "base/raw_view.hpp"
#include <base/ints.hpp>

template<typename T>
[[gnu::always_inline]]
inline static T& derefStack(std::byte* stack, i64 position) {
	return *(reinterpret_cast<T*>(&stack[position]));
}

template<typename T>
inline static T& derefView(base::ModRawView& view) {
	return *(reinterpret_cast<T*>(view.getBegin()));
}
