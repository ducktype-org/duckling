#pragma once

#include "expr.hpp"

namespace compiler::helios {

	using ExprConstructionResult = errors::HResult<base::Box<code::Expr>, errors::Failed>;

	/**
	 * @brief Construct HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 */
	DECLARE_QUERY(
		QueryHoutOfExpr, pst::GenericPSTQueryKey<pst::ExprElement>, ExprConstructionResult
	)
}
