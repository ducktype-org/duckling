#pragma once

#include "vm/bytecode/validator/type.hpp"
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates the integrity of the types in the given TypeContext.
	 * Detects any cycles in the type hierarchy (including the cycles in the inheritance
	 * hierarchy),
	 * @note Throws a builder error if the type context is invalid.
	 */

	/**
	 * @brief Validates (newly added) types in the given TypeContext.
	 * @param ctx TypeContext containing types to validate.
	 * @param new_types A vector of TypeIDs of newly added types to validate. Only these types will be validated, but the whole TypeContext is needed to perform validation.
	 */
	void validateTypes(const TypeContext& ctx, const std::vector<type::TypeID>& new_types);

	/**
	 * @brief Validates a single type in the given context.
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 *
	 * @note Throws a builder error if type is invalid in current context.
	 * @note Assumes all cycles in the hierarchy (ctx) where detected (use validateTypesIntegrity()
	 * first).
	 */
	// void validateType(
	// 	const TypeOfData&                                type,
	// 	const TypeContext&                               ctx,
	// 	const base::HashMap<base::StrID, FuncSignature>& functions
	// );
}
