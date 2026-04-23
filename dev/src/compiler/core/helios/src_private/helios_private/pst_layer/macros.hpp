#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	/**
	 * @brief Query the expansion of an expand statement.
	 *
	 * @note Ideally, this query should not be used outside of HELIOS PST-layer directory.
	 *
	 * \parallel owns its cache; creates PST via \ref pst::fromExpand (PST creation thread-safe)
	 */
	DECLARE_QUERY(
		QueryMacroExpansion,
		pst::GenericPSTQueryKey<pst::Expand>,
		query::QResult<pst::AccessLocked<pst::Stmt>>,
		({})
	)

}
