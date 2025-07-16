/**
 * @file expression_type.cpp
 * @brief deduceTypeFromExpressionType implementation
 */

#include "expression_type.hpp"

namespace tsh {
    SymbolType<> deduceTypeFromExpressionType(const ExpressionType<>& expr_type) {
		return expr_type.getSymbolType();
	}
}