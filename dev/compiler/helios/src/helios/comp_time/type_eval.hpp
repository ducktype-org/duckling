#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <typesystem/higher/type_info.hpp>

// this is needed here so contraint from GenericPSTQueryKey is satisfied:
#include <pst_parser/elements/hierarchy/expr.hpp>

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>

namespace compiler::helios {

	using TypeEval_Result = errors::HResult<tsh::TypeInfo, errors::Failed>;

	/**
	 * Given the PST expression, parses it and evaluates this expression to a type.
	 * This is a go-to API to do this.
	 * @return tsh::TypeInfo with information about the evaluated type.
	 * @todo should this return type info or type desc, we should have a document
	 * defining which one is which
	 */
	DECLARE_QUERY(EvalExprToType, pst::GenericPSTQueryKey<pst::ExprElement>, TypeEval_Result)
}
