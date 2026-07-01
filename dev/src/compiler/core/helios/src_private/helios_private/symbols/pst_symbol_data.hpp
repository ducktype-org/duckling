#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/scope_id.hpp>

namespace compiler::helios {
	/**
	 * @brief Symbol data for all symbols that are created from PST elements.
	 */
	struct PstImplementedSemantics final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		PstImplementedSemantics(ScopeID scope, pst::HashType pst_element_hash):
			  scope(scope),
			  pst_element_hash(pst_element_hash) {}

		/**
		 * Return associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const {
			return pst::LangElement::getByStableHash(pst_element_hash);
		}
	};

	enum class Builtin {
		// ...
	};

	struct BuiltinSemantics final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		Builtin builtin;

		BuiltinSemantics(ScopeID scope, pst::HashType pst_element_hash, Builtin builtin):
			  scope(scope),
			  pst_element_hash(pst_element_hash),
			  builtin(builtin) {}

		/**
		 * Return associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const {
			return pst::LangElement::getByStableHash(pst_element_hash);
		}
	};
}
