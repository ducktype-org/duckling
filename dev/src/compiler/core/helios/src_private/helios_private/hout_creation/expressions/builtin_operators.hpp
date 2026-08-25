/**
 * This file stores operations used by helios to generate builtin
 * operations to hout-expressions (like operations on int's, bools).
 * Note that those builtins are a different things then
 * helios builtin symbols.
 */

#pragma once

#include <helios/hout/elements/expr.hpp>  // @TODO: #404 relax it.
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>

#include <lexer/token_common.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/utils/simple_keys.hpp>
#include "helios/symbols/lang_primitives.hpp"

namespace compiler::helios::code {
	/**
	 * @brief Finds a numeric builtin unary operator for an expression and a given name.
	 * If necessary, returns the required coercion for the argument type to match the operator.
	 * @return Returns the operation along with coercions to apply to operands in format
	 * (builtin_operation, coercion)
	 */
	base::Optional<std::tuple<BuiltinUnary, Coercion>> findNumericUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	);

	/**
	 * @brief Finds a numeric builtin binary operator between two expressions and for a given name.
	 * If types don't match directly, checks whether one can implicitly coerce to another.
	 * @return Returns the operation with proper coercions applied to operands
	 */
	base::Optional<Box<Expr>> resolveNumericBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, Box<Expr> lhs, Box<Expr> rhs
	);

	/**
	 * @brief Represents a builtin operator which is not a numeric operator, and how it
	 * should appear in HOUT (as a BuiltinUnary, BuiltinBinary, or a function call).
	 */
	struct RegularBuiltinOperator final {
		// The symbol of the builtin operator.
		SymID symbol;

		struct FunctionCall final {
			SymID function_symbol;
		};

		using HOUTRepresentation = std::variant<BuiltinUnary, BuiltinBinary, FunctionCall>;

		// The HOUT operation to perform — either a BuiltinUnary, BuiltinBinary or a function call.
		// Note: the called function may be different from the symbol. For example, the `++`
		// operator on strings actually calls a built-in concat function under a different name.
		HOUTRepresentation op;
	};

	// Type for storing a mapping between regular builtin operator symbols and related helpful data.
	// The `symbol` in the data is the same as the key. We predict that the value type will
	// grow in complexity as we introduce more features, so we keep the symbol for convenience.
	using RegularBuiltinOperatorSymbolMap = base::StableHashMap<SymID, RegularBuiltinOperator>;

	/**
	 * @brief Get all builtin operators which are *not* numeric operators
	 * for the purpose of lookup and overload resolution. This is a query for idiomatic parallelism.
	 * @note: The symbols' implementation in
	 * compiler::helios::defgen::generateBuiltinOperatorExpression must be kept up-to-date with
	 * this list.
	 */
	DECLARE_QUERY(
		QueryRegularBuiltinOperatorSymbols,
		query::EmptyKey,
		CRef<RegularBuiltinOperatorSymbolMap>,
		({ .uses_qresult = false })
	);
}
