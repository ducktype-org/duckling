/**
 * This file stores operations used by helios to generate builtin
 * operations to hout-expressions (like operations on int's, bools).
 * Note that those builtins are a different things then
 * helios builtin symbols.
 */

#pragma once

#include <helios/hout/elements/expr.hpp>  // @TODO: #404 relax it.
#include <helios_private/hout_creation/expressions/coercions.hpp>

#include <lexer/token_common.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/utils/simple_keys.hpp>

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
	 * @brief Represents a builtin binary operator which is not a numeric operator, and how it
	 * should appear in HOUT (as a BuiltinBinary, or a function call).
	 */
	struct RegularBinaryBuiltin final {
		// The symbol of the builtin operator.
		SymID symbol;

		struct FunctionCall final {
			SymID function_symbol;
		};

		using HOUTRepresentation = std::variant<BuiltinBinary, FunctionCall>;

		// The HOUT operation to perform — either a BuiltinBinary or a function call.
		// Note: the called function may be different from the symbol. For example, the `++`
		// operator on strings actually calls a built-in concat function under a different name.
		HOUTRepresentation op;
	};

	// Type for storing a mapping between regular binary builtin symbols and related helpful data.
	// The `symbol` in the data is the same as the key. We predict that the value type will
	// grow in complexity as we introduce more features, so we keep the symbol for convenience.
	using RegularBinaryBuiltinSymbolMap = base::StableHashMap<SymID, RegularBinaryBuiltin>;

	/**
	 * @brief Get all builtin binary operators which are *not* numeric operators
	 * for the purpose of lookup and overload resolution. This is a query for idiomatic parallelism.
	 * @note: The symbols' implementation in
	 * compiler::helios::defgen::generateBuiltinOperatorExpression must be kept up-to-date with
	 * this list.
	 */
	DECLARE_QUERY(
		QueryRegularBinaryBuiltinSymbols,
		query::EmptyKey,
		CRef<RegularBinaryBuiltinSymbolMap>,
		({ .uses_qresult = false })
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
