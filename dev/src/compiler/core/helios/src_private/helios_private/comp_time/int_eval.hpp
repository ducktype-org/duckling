#pragma once

#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using NumCTVEval_Result = query::QResult<num_ctv, errors::Failed>;

	/**
	 * Query that comp-time evaluates an expresion.
	 * @note For now it only supports integer values,
	 * in the future we will introduce more generic CTV values.
	 */
	DECLARE_QUERY(EvalExprToNumCTV, pst::GenericPSTQueryKey<pst::ExprElement>, NumCTVEval_Result)
}
