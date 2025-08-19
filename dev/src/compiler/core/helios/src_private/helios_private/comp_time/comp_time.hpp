#pragma once

#include <helios/ctv/ctv.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements/expr.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>

#include <query_framework/query_result.hpp>

/**
 * @brief Main query for compile time evaluation of any type.
 * Tries evaluating with Tree Evaluation (Short Path) and if the expression is to complicated it
 * evaluates it on DVM.
 */
namespace compiler::helios {
	using CompTimeEvalResult = query::QResult<CompileTimeValue, errors::Failed>;

	DECLARE_QUERY(QueryCompTime, pst::GenericPSTQueryKey<pst::ExprElement>, CompTimeEvalResult)
}
