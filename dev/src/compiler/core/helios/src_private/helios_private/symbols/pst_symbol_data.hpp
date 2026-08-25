#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>

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

	/**
	 * @brief Symbol data for symbols declared in a class body, i.e. fields, methods,
	 * constructors and destructors.
	 */
	struct ClassMemberSemantics final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		/**
		 * Class the symbol is a member of.
		 */
		SymID owner_class;

		ClassMemberSemantics(ScopeID scope, pst::HashType pst_element_hash, SymID owner_class):
			  scope(scope),
			  pst_element_hash(pst_element_hash),
			  owner_class(owner_class) {}

		/**
		 * Return associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const {
			return pst::LangElement::getByStableHash(pst_element_hash);
		}
	};

	/**
	 * @brief Symbol data for compiler builtins, i.e. PST `fundecl`s carrying a `@builtin("...")`
	 * attribute. The declaration comes from the PST, but the implementation is provided by the
	 * compiler (see `getBuiltinImpl`) rather than from a body in the source.
	 */
	struct BuiltinSemantics final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		/**
		 * Which builtin this symbol implements.
		 */
		BuiltinKind builtin;

		BuiltinSemantics(ScopeID scope, pst::HashType pst_element_hash, BuiltinKind builtin):
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
