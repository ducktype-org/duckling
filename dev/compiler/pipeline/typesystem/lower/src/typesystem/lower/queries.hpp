#pragma once

#include <query_framework/query_int.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/lower/type_layout.hpp>

namespace tsl {
	/**
	 * @brief Get a TypeLayout for a given AbstractType.
	 */
	DECLARE_QUERY(QueryAbstractTypeLayout, tsh::AbstractType, TypeLayout)

	/**
	 * @brief Get a TypeLayout for a given SymbolType, taking reference indirection into account.
	 */
	DECLARE_QUERY(QuerySymbolTypeLayout, tsh::SymbolType<>, TypeLayout)
}
