#pragma once

#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <query_framework/query_int.hpp>

namespace compiler::tsl {
	/**
	 * @brief Get a TypeLayout for a given AbstractType.
	 *
	 * @ingroup query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryAbstractTypeLayout, tsh::AbstractType, CRef<TypeLayout>, ({ .uses_qresult = false })
	)

	/**
	 * @brief Get a TypeLayout for a given SymbolType, taking reference indirection into account.
	 */
	DECLARE_QUERY(
		QuerySymbolTypeLayout, tsh::SymbolType<>, CRef<TypeLayout>, ({ .uses_qresult = false })
	)
}
