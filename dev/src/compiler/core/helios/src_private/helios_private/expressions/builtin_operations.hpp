/**
 * This file stores operations used by helios to generate builtin
 * operations to hout-expressions (like operations on int's, bools).
 * Note that those builtins are a different things then
 * helios builtin symbols.
 */

#pragma once

#include "helios_private/expressions/coercions.hpp"

#include <helios/hout/elements/expr.hpp>  // @TODO relax it #404

#include <lexer/token_common.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Finds a builtin binary operation between two expressions and for a given operator.
	 * If types don't match directly, checks whether one can implicitly coerce to another.
	 * @return Returns the operation along with coercions to apply to operands.
	 */
	base::Optional<std::tuple<BuiltinBinary, Coercion, Coercion>> findBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	);

	/**
	 * @brief Finds a builtin unary operation for a given expression and for given operator.
	 * Returns None if no such operation exists.
	 */
	base::Optional<BuiltinUnary> findUnaryBuiltin(lexer::Operator op, CRef<Expr> expr);
}
