#pragma once

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>

#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

/**
 * @brief Main query for compile time evaluation of any type.
 * Tries evaluating with Tree Evaluation (Short Path) and if the expression is to complicated it
 * evaluates it on DVM.
 */
namespace compiler::helios {
	using CompTimeEvalResult = query::QResult<ctv::CompileTimeValue, query::Failed>;

	struct KeyFor_QueryEvaluateHOUTExpression {
		CRef<code::Expr> expr;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return expr->getID().asInt();
		}
	};

	/**
	 * @brief Evaluate a HOUT expression in compile time.
	 */
	DECLARE_QUERY(
		QueryEvaluateHOUTExpression, KeyFor_QueryEvaluateHOUTExpression, CompTimeEvalResult, ({})
	)

	/**
	 * @brief Evaluate a PST expression in compile time.
	 * @note Effectively generates the HOUT of a PST expression and
	 * evaluates it using QueryEvaluateHOUTExpression.
	 */
	DECLARE_QUERY(
		QueryEvaluatePSTExpression,
		pst::GenericPSTQueryKey<pst::ExprElement>,
		CompTimeEvalResult,
		({
			.used_hashes = query::UsedHashes::StableHash,
		})
	)

	/**
	 * Get a CTV representing a type evaluated from a PST expression.
	 * @note This is most useful for evaluating expressions where a type is expected,
	 * e.g. types in declarations or type assertions.
	 * @param ctx The query context.
	 * @param pst_expr The PST expression to evaluate to a type.
	 * @return The CTV with the type, or query::Failed if evaluation failed.
	 */
	CompTimeEvalResult getTypeCTVFromPST(
		query::Context& ctx, pst::GenericPSTQueryKey<pst::ExprElement> pst_expr
	);
}
