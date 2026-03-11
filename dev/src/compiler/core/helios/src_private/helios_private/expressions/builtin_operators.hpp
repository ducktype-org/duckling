/**
 * This file stores operations used by helios to generate builtin
 * operations to hout-expressions (like operations on int's, bools).
 * Note that those builtins are a different things then
 * helios builtin symbols.
 */

#pragma once

#include <helios/hout/elements/expr.hpp>  // @TODO: #404 relax it.
#include <helios_private/expressions/coercions.hpp>

#include <lexer/token_common.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Finds a numeric builtin binary operator between two expressions and for a given name.
	 * If types don't match directly, checks whether one can implicitly coerce to another.
	 * @return Returns the operation along with coercions to apply to operands in format
	 * (builtin_operation, left_coercion, right_coercion)
	 */
	base::Optional<std::tuple<BuiltinBinary, Coercion, Coercion>> findNumericBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	);

	/**
	 * @brief Get all builtin binary operators which are *not* numeric operators
	 * for the purpose of lookup and overload resolution.
	 * @note: The symbols' implementation in
	 * compiler::helios::houtgen::generateBuiltinOperatorExpression must be kept up-to-date with
	 * this list.
	 */
	CRef<std::vector<SymID>> getRegularBinaryBuiltinSymbols(query::Context& ctx);

	/**
	 * @brief Finds a builtin unary operation for a given expression and for given operator. If the
	 * given expression's type is not direct, performs the necessary coercion.
	 * Returns None if no such operation exists.
	 */
	base::Optional<std::tuple<BuiltinUnary, Coercion>> findUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	);
}
