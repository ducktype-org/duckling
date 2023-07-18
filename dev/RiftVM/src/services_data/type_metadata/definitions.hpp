#pragma once

#include <base/stable_container.hpp>

#include <cstdint>
#include <base/strongly_typed_int.hpp>
#include <json/json.hpp>

namespace vm {
	
	// @TODO: change to strong Id maker when it is ready
	STRONG_TYPEDEF_INT(TypeId, ::std::uint64_t);
	using Offset = std::uint64_t;
	class Type;

	using TypeRef = base::StableListRef<Type>;
	using TypeCRef = base::StableListCRef<Type>;
}