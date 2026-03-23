#pragma once

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/hout/elements/expr.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Processes a square bracket call expression (`[]`), which can be either an array type
	 * creation or an index access operation.
	 *
	 * This includes handling of:
	 * - Array Type Creation - if the base expression evaluates to a meta-type,
	 *   this function treats the call as creating a static array type (e.g., `i32[10]`).
	 * - Type template baking - if the base expression evaluates to a type template type,
	 *   this function expects the argument to be coercible to meta and treats the call as
	 * 	 specializing a type template (currently only used for specializing the builtin `List[T]`
	 * type).
	 * - Index Access - if the base expression is an array-like type, this function treats
	 *   the call as an access to an element by its index (`my_array[0]`).
	 *
	 * An error is returned if the call has an incorrect number of arguments (only one is allowed)
	 * or if the base expression is not indexable.
	 *
	 * @param ctx The query context.
	 * @param base The base expression on which the square bracket call is performed.
	 * @param call_expr The PST node representing the square bracket call (e.g., `[...]`).
	 * @return A query result containing the constructed HOUT expression (`IndexExpr`) on success,
	 *         or a failed result with logged diagnostics on error.
	 */
	query::QResult<base::Box<Expr>> processSquareCall(
		query::Context& ctx, base::Box<Expr> base, pst::Access<pst::expr::Call> call_expr
	);
}
