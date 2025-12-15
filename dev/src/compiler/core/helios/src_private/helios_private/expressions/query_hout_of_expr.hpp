#pragma once

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>

#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using ExprConstructionResult = query::QResult<Box<code::Expr>, query::Failed>;

	/**
	 * @brief Constructs a HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 * @TODO: #1362 hout 2.0: make it return ref, not box
	 */
	DECLARE_QUERY(
		QueryHoutOfExpr,
		pst::GenericPSTQueryKey<pst::ExprElement>,
		ExprConstructionResult,
		({ .used_hashes = query::UsedHashes::StableHash })
	)

	/**
	 * Constructs a HOUT Expr from a PST Expr and coerces it to the expected type.
	 * @param ctx The query context.
	 * @param pst_expr The PST expression.
	 * @param expected_type The expected type of the expression.
	 * @return A HOUT Expression of the expected type, or an error if coercion is not possible.
	 */
	ExprConstructionResult getHoutOfExprWithExpectedType(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		tsh::SymbolType<>                                expected_type
	);
}
