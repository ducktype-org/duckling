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
	 * @note Method calls are supported by providing the "self" argument as @p self_symbol. If the candidate is a method but @p self_symbol is not provided, the candidate will be considered as not matching the call and the error will be logged.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callable_expr The PST expression representing the callee being invoked.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 * @param self_symbol An expression representing the "self" argument in case of method call.
	 */
	query::QResult<Box<CallExpr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::ExprElement> callee_expr,
		pst::Access<pst::expr::Call>  call_expr,
		base::Optional<Box<Expr>>     self_symbol
	);
}
