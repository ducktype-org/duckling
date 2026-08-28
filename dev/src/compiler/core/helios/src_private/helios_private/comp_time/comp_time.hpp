#pragma once

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	using CompTimeEvalResult = query::QResult<ctv::CompileTimeValue>;

	struct KeyFor_QueryEvaluateHOUTExpression {
		CRef<code::Expr> expr;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return expr->getID().asInt();
		}
	};

	/**
	 * @brief Main query for compile time evaluation of any type.
	 * Tries evaluating with Tree Evaluation (Short Path) and if the expression is to complicated it
	 * evaluates it on DVM.
	 *
	 * \query_not_thread_safe
	 */
	DECLARE_QUERY(
		QueryEvaluateHOUTExpression, KeyFor_QueryEvaluateHOUTExpression, CompTimeEvalResult, ({})
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

	/**
	 * Evaluate a condition of an `if const` statement at compile time.
	 * @param ctx The query context.
	 * @param pst_expr The PST expression of the condition.
	 * @return The value of the condition, or query::Failed if it could not be evaluated as a
	 * compile time `bool`.
	 */
	query::QResult<bool> evaluateConstIfCondition(
		query::Context& ctx, pst::GenericPSTQueryKey<pst::ExprElement> pst_expr
	);
}
