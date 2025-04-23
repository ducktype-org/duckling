#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/hout/elements/expr.hpp>
#include <pst_parser/elements/hierarchy/expr.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::helios {

	using ExprConstructionResult = errors::HResult<base::Box<code::Expr>, errors::Failed>;

	/**
	 * @brief Construct HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 * @todo hout 2.0: make it return ref, not box
	 */
	DECLARE_QUERY(QueryHoutOfExpr, pst::GenericPSTQueryKey<pst::ExprElement>, ExprConstructionResult)
}
