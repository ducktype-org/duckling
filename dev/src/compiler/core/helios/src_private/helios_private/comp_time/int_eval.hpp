#pragma once

#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using IntEval_Result = query::QResult<i64, errors::Failed>;

	/**
	 * Query that comp-time evaluates an expresion.
	 * @note For now it only supports integer values,
	 * in the future we will introduce more generic CTV values.
	 */
	DECLARE_QUERY(EvalExprToI64, pst::GenericPSTQueryKey<pst::ExprElement>, IntEval_Result)
}
