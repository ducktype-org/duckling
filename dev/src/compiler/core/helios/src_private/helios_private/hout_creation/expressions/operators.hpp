/**
 * This file stores operations used by helios to generate builtin
 * operations to hout-expressions (like operations on int's, bools).
 * Note that those builtins are a different things then
 * helios builtin symbols.
 */

#pragma once

#include <helios/hout/elements/expr.hpp>  // @TODO: #404 relax it.
#include <helios/hout/hout.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>

#include <lexer/token_common.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/utils/simple_keys.hpp>

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
	 * @brief Finds the appropriate binary operator to call and constructs the corresponding
	 * HOUT expression. Consumes the provided expressions of the arguments.
	 * Currently used for all operators other than `As` (type cast) and `Pipe` (variant type
	 * construction). Perhaps they will be moved here later.
	 * @param ctx The context of the query
	 * @param op The operator
	 * @param op_origin The origin of the operator in the PST
	 * @param lhs The precomputed left-hand side argument
	 * @param rhs The precomputed right-hand side argument
	 * @param scope The scope in which the operator call happens
	 */
	[[nodiscard]]
	Box<Expr> resolveBinaryOperator(
		query::Context& ctx,
		lexer::Operator op,
		ElementOrigin   op_origin,
		Box<Expr>       lhs,
		Box<Expr>       rhs,
		ScopeID         scope
	);

	/**
	 * @brief Finds the appropriate unary operator to call and constructs the corresponding
	 * HOUT expression. Consumes the provided argument expression.
	 * Some cases, such as the ampersand and asterisk for references are not handled here.
	 * Perhaps they will be moved here later.
	 * @param ctx The context of the query
	 * @param op The operator
	 * @param op_origin The origin of the operator in the PST
	 * @param inner The precomputed argument
	 * @param scope The scope in which the operator call happens
	 * @param operatoriness Whether the operator is prefix or suffix
	 */
	[[nodiscard]]
	Box<Expr> resolveUnaryOperator(
		query::Context&                              ctx,
		lexer::Operator                              op,
		ElementOrigin                                op_origin,
		Box<Expr>                                    inner,
		const ScopeID                                scope,
		const HOUTFunctionDeclaration::Operatoriness operatoriness
	);

	/**
	 * @brief Check if an operator allows their arguments to undergo numeric promotion.
	 * @param op The operator to check.
	 * @return Whether the operator is numeric.
	 */
	bool isNumericOperator(const lexer::Operator op);

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
