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
