// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/const_value.hpp>
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
		Identifier                      global_name
	);
}
