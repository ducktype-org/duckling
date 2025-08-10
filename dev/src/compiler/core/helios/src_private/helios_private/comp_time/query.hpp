#pragma once

#include <helios/ctv/ctv.hpp>
#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

/**
 * @brief Main query for compile time evaluation. For now evaluates only with TreeEval, in the
 * future it will use VM evaluation as well.
 */
namespace compiler::helios {

	using CompTimeEvalResult = query::QResult<CompileTimeValue, errors::Failed>;

	DECLARE_QUERY(
		EvaluateAtCompileTime, pst::GenericPSTQueryKey<pst::ExprElement>, CompTimeEvalResult
	)
}
