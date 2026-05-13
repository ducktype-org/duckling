#pragma once
#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/core/fast/program/type.hpp>

namespace vm::fast::detail {
	void rebuildFastTypeCollection(
		Ref<TypeCollection> type_collection, const vm::code::valid_type::ValidTypeMap& types
	);
}
