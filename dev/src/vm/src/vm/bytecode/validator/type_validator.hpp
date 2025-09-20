#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_context.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates the integrity of the types in the given TypeContext.
	 * Detects any cycles in the type hierarchy (including the cycles in the inheritance
	 * hierarchy),
	 * @note Throws a builder error if the type context is invalid.
	 */
	void validateTypesIntegrity(const TypeContext& ctx);

	/**
	 * @brief Validates a single type in the given context.
	 * For the complete list of specific checks see `vm/src/vm/bytecode/validator/readme.md`.
	 *
	 * @note Throws a builder error if type is invalid in current context.
	 * @note Assumes all cycles in the hierarchy (ctx) where detected (use validateTypesIntegrity()
	 * first).
	 */
	void validateType(
		const TypeOfData&                                type,
		const TypeContext&                               ctx,
		const base::HashMap<base::StrID, FuncSignature>& functions
	);
}
