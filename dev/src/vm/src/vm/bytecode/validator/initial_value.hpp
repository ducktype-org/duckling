#pragma once

#include <vm/bytecode/const_pool.hpp>
#include <vm/bytecode/validator/valid_type/type_map.hpp>

namespace vm::code::detail {
	/**
	 * @brief Validates that a ConstantValue matches the expected ValidType.
	 * Throws InitialValueTypeMismatchError if the value does not conform.
	 * @param value The constant value to validate.
	 * @param expected_type_id The ValidTypeID of the expected type.
	 * @param types The map of all finalized types.
	 * @param global_name The name of the global variable (for error messages).
	 */
	void validateInitialValue(
		const ConstantValue&            value,
		valid_type::ValidTypeID         expected_type_id,
		const valid_type::ValidTypeMap& types,
		base::StrID                     global_name
	);
}
