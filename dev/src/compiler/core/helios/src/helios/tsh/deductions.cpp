// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file deductions.cpp
 * @brief Implementation of type deduction utilities.
 */

#include "deductions.hpp"

namespace compiler::tsh::deductions {
	ValueType<> declarationTypeFromInitializer(
		const ExpressionType<>& expr_type, const Mutability expected_mutability
	) {
		return ValueType<>::withDefaults(expr_type.getValueType().getType())
		    .withMutability(expected_mutability);
	}

	ValueType<> declarationTypeFromProvidedType(
		const ValueType<>& given_type, const Mutability expected_mutability
	) {
		return given_type.withMutability(expected_mutability);
	}
}
