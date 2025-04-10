#pragma once

#include <base/ints.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_int.hpp>

namespace vm {
	STRONG_TYPEDEF_INT(TypeID, usize);
	using Offset = u64;
	class Type;

	using TypeRef  = Ref<Type>;
	using TypeCRef = CRef<Type>;
}

template<>
struct std ::hash<vm ::TypeID> final {
	usize operator()(const vm ::TypeID& key) const { return static_cast<usize>(key); }
};
