#pragma once


#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * Constructs a HOUT Expr from a PST Expr.
	 *
	 * This is an effective implementation of QueryHoutOfExpr.
	 * QueryHoutOfExpr is mostly a wrapper for future cache.
	 *
	 * @warning This function should only be used for nested expression building from the expression
	 * building context or the `QueryHoutOfExpr` entry point.
	 */
	query::QResult<Box<code::Expr>> subExprFromPST(
		query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
	);


	/**
	 * Constructs a HOUT Expr from a PST Expr.
	 *
	 * This is an effective implementation of QueryHoutOfExpr.
	 * QueryHoutOfExpr is mostly a wrapper for future cache.
	 *
	 * @warning This function should only be used for nested expression building from the expression
	 * building context or the `QueryHoutOfExpr` entry point.
	 */
	query::QResult<Box<code::Expr>> subExprFromPSTWithType(
		query::Context&                         ctx,
		pst::AccessLocked<pst::ExprElement>     element,
		tsh::SymbolType<>                       expected_type,
		base::Optional<dia_int::StablePosition> coercion_expects_pos = {},
		CoercionErrorOverrides                  error_overrides      = {}
	);
}
