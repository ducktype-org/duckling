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
	SymbolType<> declarationTypeFromInitializer(
		const ExpressionType<>& expr_type, const Mutability expected_mutability
	) {
		return expr_type.getSymbolType().withMutability(expected_mutability);
	}

	SymbolType<> declarationTypeFromProvidedType(
		const SymbolType<>& given_type, const Mutability expected_mutability
	) {
		return given_type.withMutability(expected_mutability);
	}
}
