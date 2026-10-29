// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/types.hpp>

namespace compiler::helios {
	/**
	 * @brief Symbol data for all symbols that are declared in the code created from PST elements.
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
	 * @brief Symbol data for all symbols that are created from PST elements but not directly
	 * declared. e.g. symbols like loop iterators. Their parent statements
	 * usually have some other symbol declared and associated with the parent, so those are sort of
	 * byproduct symbols. The semantics are very similar to PstImplementedSemantics, but associated
	 * PST elements are not statements.
	 */
	struct PstInferredSemantics final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * Hash of the PST element that the symbol was created from.
		 */
		pst::HashType pst_element_hash;

		PstInferredSemantics(ScopeID scope, pst::HashType pst_element_hash):
			  scope(scope),
			  pst_element_hash(pst_element_hash) {}

		/**
		 * Return associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const {
			return pst::LangElement::getByStableHash(pst_element_hash);
		}

		/**
		 * Return the parent of associated pst_element.
		 */
		[[nodiscard]] pst::AccessLocked<pst::LangElement> getParent(query::Context& ctx) const {
			auto parent_opt
				= pst::LangElement::getByStableHash(pst_element_hash).unlock(ctx)->getParent();
			CORE_ASSERT(
				parent_opt.has_value(), "PST element without a parent in PstInferredSemantics"
			);
			return parent_opt.value();
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
		tsh::ClassAbstractType owner_class;

		ClassMemberSemantics(
			ScopeID scope, pst::HashType pst_element_hash, tsh::ClassAbstractType owner_class
		):
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
