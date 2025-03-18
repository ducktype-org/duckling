#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <typesystem/higher/symbol_type.hpp>

// this is needed here so constraint from GenericPSTQueryKey is satisfied:
#include <pst_parser/elements/hierarchy/expr.hpp>

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>

namespace compiler::helios {

	using TypeEval_Result = errors::HResult<tsh::SymbolType<>, errors::Failed>;

	/**
	 * Given the PST expression, parses it and evaluates this expression to a type.
	 * This is a go-to API to do this.
	 * @return tsh::SymbolType with information about the evaluated type.
	 */
	DECLARE_QUERY(EvalExprToType, pst::GenericPSTQueryKey<pst::ExprElement>, TypeEval_Result)
}
