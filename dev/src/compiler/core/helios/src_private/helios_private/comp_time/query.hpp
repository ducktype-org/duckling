#pragma once

#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include "helios/ctv/ctv.hpp"
#include "helios_private/comp_time/ctv.hpp"
#include "../../../helios/ctv/ctv.hpp"

namespace compiler::helios {

	using CompTimeEvalResult = query::QResult<CompileTimeValue, errors::Failed>;

	DECLARE_QUERY(
		EvaluateAtCompileTime, pst::GenericPSTQueryKey<pst::ExprElement>, CompTimeEvalResult
	)
}
