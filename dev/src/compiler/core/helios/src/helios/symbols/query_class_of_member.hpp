#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	/**
	 * @brief Query the class of a member symbol.
	 * Panics if the given PST element is not a class member.
	 *
	 * @note For now used only for getting the class of a method
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryClassOfMember, SymID, CRef<SymID>, ({ .uses_qresult = false }));
}
