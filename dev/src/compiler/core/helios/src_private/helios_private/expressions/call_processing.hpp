#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <helios/helios_errors.hpp>
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
	 * @param call_expr The PST call expression representing the function call.
	 * @TODO:  #1029 implement overloading. Most of logic is implemented, make attempt. Fitting
	 * function not destroy containers. Currently candidates must have size 1. Requires #1309.
	 */
	query::QResult<Box<CallExpr>, errors::Failed> processFunctionCall(
		query::Context&              ctx,
		const std::vector<SymID>&    candidates,
		pst::Access<pst::expr::Call> call_expr
	);
}
