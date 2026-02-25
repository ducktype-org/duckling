#pragma once

#include <vm/utils/stable_obj_id_name_map.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates (newly added) types in the given TypeContext.
	 * @note Throws a builder error if type is invalid in current context.
	 * @note There is a single check that is not performed here - Cyclic dependencies between types.
	 * This is done in type::Type. For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 * @param types_ctx Type context containing types that will be validated, but also the others.
	 * @param new_types_id A vector of IDs of newly added types to validate. Only these types will
	 * be validated, but the whole TypeContext is needed to perform validation.
	 * @param functions A map of function signatures in the current context. Needed to validate
	 * classes.
	 */
	void validateTypes(
		const ObjIdNameMap<TypeOfData>&                  types_ctx,
		const std::vector<usize>&                        new_types_id,
		const base::HashMap<base::StrID, FuncSignature>& functions
	);
}
