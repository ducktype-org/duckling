#pragma once

#include <helios/scope_symbol_id.hpp>
#include <frontend/pst_parser/access.hpp>

namespace compiler::helios {
	/**
	 * @brief Symbol data for all symbols that are created from PST elements.
	 */
	struct PstSymbolData final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * PST element that the symbol was created from.
		 */
		pst::AccessLocked<pst::LangElement> pst_element;
	};
}
