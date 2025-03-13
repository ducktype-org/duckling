#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <query_framework/query_int.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <pst_parser/elements/hierarchy/expr.hpp> // this is needed here so constraint from GenericPSTQueryKey is satisfied

namespace compiler::helios {

	using TypeEval_Result = errors::HResult<tsh::AbstractType, errors::Failed>;

	/**
	 * Given the PST expression, parses it and evaluates this expression to a type.
	 * This is a go-to API to do this.
	 * @return tsh::AbstractType with information about the evaluated type.
	 * @todo should this return an AbstractType or ExpressionType, we should have a document
	 * defining which one is which
	 */
	DECLARE_QUERY(EvalExprToType, pst::GenericPSTQueryKey<pst::ExprElement>, TypeEval_Result)
}
