#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::ls {
	/**
	 * If expression is an identifier expression, returns its symbol ID.
	 * otherwise returns an empty optional.
	 */
	CRef<query::QResult<Box<code::Expr>>> getHoutExpr(
		query::Context& ctx, pst::Access<pst::ExprElement> element
	);
}
