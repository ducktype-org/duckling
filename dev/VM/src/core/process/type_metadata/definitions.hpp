#pragma once

#include "base/strongly_typed_int.hpp"
#include <base/strongly_typed_id.hpp>
#include <base/stable_container.hpp>

#include <base/ints.hpp>

namespace vm {
	STRONG_TYPEDEF_INT_DIMENSIONAL(TypeID, u64);
	using Offset = u64;
	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;
}

ID_STD_HASH(vm::TypeID);
