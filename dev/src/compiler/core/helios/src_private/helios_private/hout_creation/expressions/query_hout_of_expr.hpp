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
	 * @param coercion_expects_pos If not empty, it adds an underline with 'expects' message.
	 * @param error_overrides Optional overrides of the default coercion error logging.
	 * @return A HOUT Expression of the expected type, or an error if coercion is not possible.
	 */
	query::QResult<BoxOrCRef<code::Expr>> getHoutOfExprWithExpectedType(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		tsh::SymbolType<>                                expected_type,
		base::Optional<dia_int::StablePosition>          coercion_expects_pos = {},
		CoercionErrorOverrides                           error_overrides      = {}
	);

	/**
	 * Constructs a HOUT Expr from a PST Expr and coerces it to one of the accepted types.
	 * The types are tried in the given order and the first valid coercion is used, so more than
	 * one of them matching is not an error.
	 * @param ctx The query context.
	 * @param pst_expr The PST expression.
	 * @param expected_types The accepted types, ordered by preference. Must not be empty.
	 * @return A HOUT Expression coerced to the first matching type, or an error if none of the
	 * accepted types matches. In the latter case an error listing all the accepted types together
	 * with the reason each of their coercions failed is logged.
	 */
	query::QResult<BoxOrCRef<code::Expr>> getHoutOfExprWithExpectedTypes(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		const std::vector<tsh::SymbolType<>>&            expected_types
	);
}
