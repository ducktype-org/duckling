#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type/type_context.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates (newly added) types in the given TypeContext.
	 * @note Throws a builder error if type is invalid in given context.
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 * @param tod_types Type context containing types that will be validated, but also the others.
	 * @param new_types_id A vector of IDs of newly added types to validate. Only these types will
	 * be validated, but the whole TypeContext is needed to perform validation.
	 * @param functions A map of function signatures in the current context. Needed to validate
	 * classes.
	 */
	void validateTypes(
		const ObjIdNameMap<TypeOfData>&                  tod_types,
		const std::vector<usize>&                        new_types_id,
		const base::HashMap<base::StrID, FuncSignature>& functions
	);
}
