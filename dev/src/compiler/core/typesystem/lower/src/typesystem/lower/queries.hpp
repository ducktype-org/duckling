#pragma once

#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <query_framework/query_int.hpp>

namespace compiler::tsl {
	/**
	 * @brief Get a TypeLayout for a given AbstractType.
	 */
	DECLARE_QUERY(QueryAbstractTypeLayout, tsh::AbstractType, CRef<TypeLayout>)

	/**
	 * @brief Get a TypeLayout for a given SymbolType, taking reference indirection into account.
	 */
	DECLARE_QUERY(QuerySymbolTypeLayout, tsh::SymbolType<>, CRef<TypeLayout>)
}
