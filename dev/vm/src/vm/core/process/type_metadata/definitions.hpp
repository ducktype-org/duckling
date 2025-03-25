#pragma once

#include "base/strongly_typed_id.hpp"
#include <base/stable_container.hpp>

#include <base/ints.hpp>
#include <base/strongly_typed_int.hpp>

namespace vm {
	// @TODO: change to strong ID maker when it is ready
	STRONG_TYPEDEF_INT_DIMENSIONAL(TypeID, u64);
	using Offset = u64;
	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;
}

template<>
struct std ::hash<vm ::TypeID> final {
	usize operator()(const vm ::TypeID& key) const { return static_cast<usize>(key); }
};
