/**
 * @brief deduceTypeFromExpressionType implementation
 */

#include "expression_type.hpp"

namespace tsh {
	SymbolType<> getDeclarationTypeFromExpressionType(
		const ExpressionType<>& expr_type, const Mutability expected_mutability
	) {
		return expr_type.getSymbolType().withMutability(expected_mutability);
	}
}
