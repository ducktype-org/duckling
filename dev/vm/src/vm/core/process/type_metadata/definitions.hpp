#pragma once

#include <base/ints.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_id.hpp>

namespace vm {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);
	using Offset = u64;
	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;
}
