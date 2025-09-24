#pragma once

#include <helios/helios_errors.hpp>
#include <helios/hout/elements/expr.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>

#include <base/box.hpp>
#include <base/optional.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {

	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given call expression and creates callexpr from it. The function is selected based on
	 * argument types and named arguments. If no function or multiple functions match the call, an
	 * error is returned.
	 * @param candidates Contains all candidate functions that could be called.
	 * @param call_expr The PST call expression representing the function call.
	 * @TODO: #1300 #1029 implement overloading. Most of logic is implemented, make attempt. Fitting function
	 * not destroy containers. Currently candidates must have size 1. Requires #1300
	 */
	query::QResult<Box<CallExpr>, errors::Failed> processFunctionCall(
		query::Context&              ctx,
		const std::vector<SymID>&    candidates,
		pst::Access<pst::expr::Call> call_expr
	);
}
