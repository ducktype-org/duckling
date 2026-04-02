#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	template<typename Element>
	using ExpansionError = std::tuple<pst::AccessLocked<Element>, const Ref<dia_int::Logger>>;
	template<typename Element>
	using ExpansionResult
		= query::QResult<std::variant<pst::AccessLocked<Element>, ExpansionError<Element>>>;


	/**
	 * @brief Query the expansion of an expand statement.
	 *
	 * @note This will have some issues for now. The potential errors from parsed subexpression
	 * aren't available for now. There needs to be a small rework of errors and position first.
	 *
	 * @note Ideally, this query should not be used outside of HELIOS PST-layer directory.
	 *
	 * \parallel owns its cache; creates PST via \ref pst::fromExpand (PST creation thread-safe)
	 * \query_not_thread_safe
	 */
	DECLARE_QUERY(
		QueryMacroExpansion,
		pst::GenericPSTQueryKey<pst::Expand>,
		ExpansionResult<pst::Stmt>,
		({
			.uses_qresult = false,
		})
	)

}
