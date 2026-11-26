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
	SymbolType declarationTypeFromInitializer(
		const ExpressionType<>& expr_type, const PlaceAccess place_access
	) {
		return { ValueType<>::direct(expr_type.getValueType().getType()), place_access };
	}

	SymbolType declarationTypeFromProvidedType(
		const ValueType<>& given_type, const PlaceAccess place_access
	) {
		return { given_type, place_access };
	}
}
