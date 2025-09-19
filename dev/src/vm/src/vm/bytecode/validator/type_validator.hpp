#pragma once


#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates the types in the given TypeContext.
	 * Firstly, detects any cycles in the type hierarchy (including the cycles in the inheritance
	 * hierarchy), then checks each type individually and inheritance hierarchy soundness. Throws a
	 * builder error if types are invalid in current context.
	 *
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 */
	// TODOP: Check for cycles in the hierarchy.
	void validateTypesIntegrity(const TypeContext& ctx);

	/*
	 * @brief Validate a single given type.
	 * @note Throws a builder error if type is invalid in current context.
	 * @note Assumes all cycles in the hierarchy (ctx) where detected.
	 */
	void validateType(
		const TypeOfData&                                type,
		const TypeContext&                               ctx,
		const base::HashMap<base::StrID, FuncSignature>& functions
	);
}
