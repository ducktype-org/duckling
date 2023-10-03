#pragma once

#include <base/ints.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_int.hpp>
#include <json/json.hpp>

namespace vm {

	// @TODO: change to strong Id maker when it is ready
	STRONG_TYPEDEF_INT_DIMENSIONAL(TypeId, u64);
	using Offset = u64;
	class Type;

	using TypeRef  = base::StableListRef<Type>;
	using TypeCRef = base::StableListCRef<Type>;
}  // namespace vm
