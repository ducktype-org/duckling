#pragma once

#include <helios/tsh/abstract_type.hpp>
#include <tsl/type_layout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::tsl {
	/**
	 * @brief Get a TypeLayout for a given AbstractType.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryAbstractTypeLayout, tsh::AbstractType, CRef<query::QResult<TypeLayout>>, ({}))

	/**
	 * @brief Get a TypeLayout for a given SymbolType, taking reference indirection into account.
	 *
	 * \query_thread_safe_if_cache
	 * This query uses QueryAbstractTypeLayout directly, hence the grouping.
	 */
	DECLARE_QUERY(QuerySymbolTypeLayout, tsh::SymbolType<>, CRef<query::QResult<TypeLayout>>, ({}))
}
