#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief Builds TypeMetadata from TypeContext without validation. Assumes that the passed
	 * TypeContext was validated by `validateTypes` beforehand (in particular, there are no cycles
	 * in the inheritance hierarchy).
	 */
	Box<TypeMetadata> buildTypes(const TypeContext& ctx);
}
