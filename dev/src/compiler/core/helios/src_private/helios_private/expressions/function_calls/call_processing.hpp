#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <helios/hout/elements/expr.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given call expression and creates callexpr from it. The function is selected based on
	 * argument types and named arguments. If no function or multiple functions match the call, an
	 * error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callee_expr The PST expression representing the callee being invoked.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 */
	query::QResult<Box<Expr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::ExprElement> callee_expr,
		pst::Access<pst::expr::Call>  call_expr
	);

	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given operator expression and creates callexpr from it. The function is selected based on
	 * argument types only. If no function or multiple functions match the call, an
	 * error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param lhs The preprocessed left-hand side argument of the operator call.
	 * @param rhs The preprocessed right-hand side argument of the operator call.
	 */
	query::QResult<Box<Expr>> processBinaryOperatorCall(
		query::Context&            ctx,
		const std::vector<SymID>&  candidates,
		Box<pst::ExprElement>      lhs,
		Box<pst::ExprElement>      rhs
	);
}
