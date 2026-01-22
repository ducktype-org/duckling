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
	 * @brief Finds a builtin binary operation between two expressions and for a given operator.
	 * If types don't match directly, checks whether one can implicitly coerce to another.
	 * @return Returns the operation along with coercions to apply to operands in format
	 * (builtin_operation, left_coercion, right_coercion)
	 */
	base::Optional<std::tuple<BuiltinBinary, Coercion, Coercion>> findBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	);

	/**
	 * @brief Finds a builtin unary operation for a given expression and for given operator. If the
	 * given expression's type is not direct, performs the necessary coercion.
	 * Returns None if no such operation exists.
	 */
	base::Optional<std::tuple<BuiltinUnary, Coercion>> findUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	);
}
