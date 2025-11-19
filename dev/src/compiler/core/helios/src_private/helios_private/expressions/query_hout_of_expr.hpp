#pragma once

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using ExprConstructionResult = query::QResult<Box<code::Expr>, errors::Failed>;

	/**
	 * @brief Construct HOUT Expr from Pst Expr.
	 * @note This will likely panic for non-top expression in the future.
	 * @TODO: #1362 hout 2.0: make it return ref, not box
	 */
	DECLARE_QUERY(
		QueryHoutOfExpr, pst::GenericPSTQueryKey<pst::ExprElement>, ExprConstructionResult, ({.used_hashes =
			query::internal::QueryTags::UsedHashes::StableHash})
	)
}
