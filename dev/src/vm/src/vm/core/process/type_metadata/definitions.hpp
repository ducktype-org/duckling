#pragma once

#include <base/types/ints.hpp>
#include <base/collections/stable_container.hpp>
#include <base/strongly_typed_id.hpp>

namespace vm {
	using Offset = u64;
	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;

	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(GlobalDataID);
}

ID_STD_HASH(vm::TypeID);
ID_STD_HASH(vm::GlobalDataID);
