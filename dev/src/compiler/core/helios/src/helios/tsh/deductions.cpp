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
		const ExpressionType<>& expr_type,
		const Mutability        expected_mutability,
		const bool              initializer_takes_reference
	) {
		const auto& initializer_type = expr_type.getSymbolType();
		const bool  strip_reference  = initializer_type.getRefKind() == ReferenceKind::Ref
		                         and not initializer_takes_reference;
		const auto declared_type = strip_reference ? initializer_type.toDirect() : initializer_type;
		return declared_type.withMutability(expected_mutability);
	}

	SymbolType<> declarationTypeFromProvidedType(
		const SymbolType<>& given_type, const Mutability expected_mutability
	) {
		return given_type.withMutability(expected_mutability);
	}
}
