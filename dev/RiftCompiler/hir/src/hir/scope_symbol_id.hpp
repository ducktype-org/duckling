// this file is @deprecated

#pragma once

#include <base/strongly_typed_id.hpp>

namespace symtable {
	class Scope;
	class Symbol;

	/**
	 * @brief Symbol ID is currently left only for TypeSystem (@TODO: substitute for SymbolRef)
	 * @TODO: Once we remove SymbolId, this should be removed.
	 * @deprecated
	 */
	STRONG_TYPEDEF_ID(SymbolId);
}
