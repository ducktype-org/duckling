#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/ctv/ctv.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

/**
 * @brief Main query for compile time evaluation of any type.
 * Tries evaluating with Tree Evaluation (Short Path) and if the expression is to complicated it
 * evaluates it on DVM.
 */
namespace compiler::helios {
	using CompTimeEvalResult = query::QResult<CompileTimeValue, errors::Failed>;

	DECLARE_QUERY(
		QueryEvaluateExpression,
		pst::GenericPSTQueryKey<pst::ExprElement>,
		CompTimeEvalResult,
		({ .used_hashes = query::UsedHashes::StableHash })
	)
}
