#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	// @TODO: #2111 This is a temporary solution. Refactor class members symbol data to be able to
	// access the class directly from the symbol data, without having to go through the scope and
	// PST element.
	/**
	 * @brief Query the class of a member symbol.
	 * Panics if the given PST element is not a class member.
	 *
	 * @note For now used only for getting the class of a method
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryClassOfMember, SymID, CRef<query::QResult<tsh::ClassAbstractType>>, ({}));
}
