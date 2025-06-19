#pragma once

#include <helios/helios_errors.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {

	using TypeEval_Result = query::QResult<tsh::SymbolType<>, errors::Failed>;

	/**
	 * Given the PST expression, parses it and evaluates this expression to a type.
	 * This is a go-to API to do this.
	 * @return tsh::SymbolType with information about the evaluated type.
	 */
	DECLARE_QUERY(EvalExprToType, pst::GenericPSTQueryKey<pst::ExprElement>, TypeEval_Result)
}
