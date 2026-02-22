#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/function_calls/call_source_positions.hpp>

#include <base/pointers/box.hpp>

namespace compiler::helios::houtgen {
	/**
	 * @brief Generate the HOUT expression which evaluates a builtin, non-numeric operator.
	 * @note: Implementation must be kept up-to-date with the list of symbols given by
	 * compiler::helios::code::getRegularBinaryBuiltinSymbols
	 */
	Box<code::Expr> generateBuiltinOperatorExpression(
		query::Context&                              ctx,
		const CallSourcePositions&                   source_positions,
		SymID                                        operator_symbol,
		std::vector<Box<code::Expr>>                 arguments,
		const base::Optional<std::vector<Coercion>>& coercions
	);
}
