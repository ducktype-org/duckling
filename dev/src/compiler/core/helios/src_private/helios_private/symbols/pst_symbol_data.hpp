#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/scope_id.hpp>

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
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		PstSymbolData(ScopeID scope, pst::HashType pst_element_hash):
			  scope(scope),
			  pst_element_hash(pst_element_hash) {}

		/**
		 * Return associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const {
			return pst::LangElement::getByStableHash(pst_element_hash);
		}
	};
}
