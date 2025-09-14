#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief Validates the types in the given TypeContext.
	 * Firstly, detects any cycles in the type hierarchy (including the cycles in the inheritance
	 * hierarchy), then checks each type individually and inheritance hierarchy soundness. Throws a
	 * builder error if types are invalid in current context.
	 *
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`).
	 */
	void validateTypes(const TypeContext& ctx);
}
