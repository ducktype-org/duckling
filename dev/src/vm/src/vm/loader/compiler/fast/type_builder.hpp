#pragma once
#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/core/fast/program/type.hpp>

namespace vm::loader::compiler::fast {
	void rebuildFastTypeCollection(
		Ref<vm::fast::TypeCollection> type_collection, const vm::code::valid_type::ValidTypeMap& type_ctx
	);
}
