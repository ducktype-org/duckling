#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <typesystem/higher/type_info.hpp>

// this is needed here so contraint from GenericPSTQueryKey is satisfied:
#include <pst_parser/elements/hierarchy/expr.hpp>

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>

namespace compiler::helios {

	using IntEval_Result = errors::HResult<i64, errors::Failed>;

	/**
	 * Query that comp-time evaluates an expresion.
	 * @note For now it only supports integer values,
	 * in the future we will introduce more generic CTV values.
	 */
	DECLARE_QUERY(EvalExprToInt, pst::GenericPSTQueryKey<pst::ExprElement>, IntEval_Result)
}
