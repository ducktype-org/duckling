#pragma once

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>

#include <base/pointers/box_or_ref.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using ExprConstructionResult = query::QResult<Box<code::Expr>>;

	/**
	 * @brief Constructs a HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(
		QueryHoutOfExpr,
		pst::GenericPSTQueryKey<pst::ExprElement>,
		CRef<ExprConstructionResult>,
		({})
	)

	/**
	 * Constructs a HOUT Expr from a PST Expr and coerces it to the expected type.
	 * @param ctx The query context.
	 * @param pst_expr The PST expression.
	 * @param expected_type The expected type of the expression.
	 * @param error_overrides Optional overrides of the default coercion error logging.
	 * @return A HOUT Expression of the expected type, or an error if coercion is not possible.
	 */
	query::QResult<BoxOrCRef<code::Expr>> getHoutOfExprWithExpectedType(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		tsh::SymbolType<>                                expected_type,
		CoercionErrorOverrides                           error_overrides = {}
	);
}
