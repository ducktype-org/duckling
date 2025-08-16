#pragma once

#include "helios/hout/elements/expr.hpp"

#include <helios/ctv/ctv.hpp>
#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>

#include "query_framework/query_hash.hpp"
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <functional>

/**
 * @brief Main query for compile time evaluation. For now evaluates only with TreeEval, in the
 * future it will use VM evaluation as well.
 */
// TODOP: Fix comment.
namespace compiler::helios {

	using CompTimeEvalResult = query::QResult<CompileTimeValue, errors::Failed>;

	struct HoutExprKey {
		const code::Expr* expr;

		auto operator<=>(const HoutExprKey&) const = default;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			static base::Map<HoutExprKey, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	/**
	 * @brief Internal query for evaluateing HOUT expressions. Used by `QueryEvaluateExpressionCT`.
	 */
	DECLARE_QUERY(QueryEvaluateHoutExpressionCT, HoutExprKey, CompTimeEvalResult)

	/**
	 * @brief Top level query for evaluating PST expressions at compile time.
	 */
	DECLARE_QUERY(
		QueryEvaluateExpressionCT, pst::GenericPSTQueryKey<pst::ExprElement>, CompTimeEvalResult
	)

}
