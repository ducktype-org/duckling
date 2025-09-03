#pragma once

#include <helios_private/lookup/interface.hpp>

#include <base/optional.hpp>
#include <base/box.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <helios/hout/elements/expr.hpp>




namespace compiler::helios::code {

	/**
	 * @brief Determines the correct function to call from the given call expression.
	 * The function is selected based on argument types and named arguments.
	 * If no function or multiple functions match the call, nullopt is returned.
	 * @param lookup_result Contains all candidate functions that could be called.
	 * @param call_expr The PST call expression representing the function call.
	*/
	base::Optional<Box<CallExpr>> processFunctionCall(
		query::Context& ctx, CRef<LookupResult> lookup_result, pst::Access<pst::expr::Call> call_expr
	);
}
