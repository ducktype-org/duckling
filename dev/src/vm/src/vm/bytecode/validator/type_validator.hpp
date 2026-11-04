// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates (newly added) types in the given TypeContext.
	 * @note Throws a builder error if type is invalid in given context.
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 * @param types_ctx Type map containing types that will be validated, but also all the other
	 * already injected types.
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
