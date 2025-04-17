#pragma once

#include <helios/hout/elements/expr.hpp>
#include <query_framework/query_int.hpp>
#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <pst_parser/elements/hierarchy/expr.hpp>

// @todo this file should not be here
// we might want to review HELIOS folder structure in general,
// after HOUT 2.0

namespace compiler::helios {

	using ExprConstructionResult = errors::HResult<base::Box<code::Expr>, errors::Failed>;

	/**
	 * @brief Construct HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 * @todo hout 2.0: make it return ref, not box
	 */
	DECLARE_QUERY(
		QueryHoutOfExpr, pst::GenericPSTQueryKey<pst::ExprElement>, ExprConstructionResult
	)
}
