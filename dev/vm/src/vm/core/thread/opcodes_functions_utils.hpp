#pragma once

#include <base/ints.hpp>
#include <base/raw_view.hpp>

template<typename T>
[[gnu::always_inline]]
inline static T& derefStack(std::byte* stack, i64 position) {
	return *(reinterpret_cast<T*>(&stack[position]));
}

template<typename T>
inline static T& derefView(base::ModRawView view) {
	return *(reinterpret_cast<T*>(view.getBegin()));
}

/**
 * Returns a reference of type TYPE (eg. int, i64, usize. etc) to a global data with id ID.
 */
#define DEREF_GLOBAL(TYPE, ID) \
	derefView<TYPE>(thread.process_memory.getGlobalData(GlobalDataID(usize(ID))))
