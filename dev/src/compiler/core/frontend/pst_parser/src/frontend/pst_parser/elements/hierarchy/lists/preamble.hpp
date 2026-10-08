// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief General Element representing a list of Elements.
	 *
	 * @tparam ListElements - Kept Elements, has to have precise length parse like Expr
	 * @tparam getName - List name getter for errors.
	 * @tparam Container - Vector-like container of SubElements with emplace_back. Possibly with
	 * other condition because of iteration.
	 */
	template<
		class ListElements,
		GetName getName,
		class Container = std::vector<AccessInternalAnonymous<ListElements>>>
	class List: public NotStmt {
		THIS_CLASS(List);
		PARENT_CLASS(NotStmt);

	protected:
		Container elements;

	public:
		friend class ListParsingTemplate;

		DECLARE_CONST_ELEMENT_ITERATOR(elements, ListElements)

		[[nodiscard]]
		usize size() const {
			return elements.size();
		}

		ELEMENT_CLONE_DECL(List)

		explicit List(const LangParserState& state): NotStmt(state) {}

		[[nodiscard]]
		std::string elementType() const override {
			return getName() + " list";
		}

		void dprint(std::ostream& out) const final {
			out << "[";
			for (auto& x: elements) {
				nullAwareDprint(x, out);
				out << ",";
			}
			out << "]";
		}

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override {
			addToHash(partial_hash, elements.size());
			return partial_hash;
		}

		/**
		 * @brief A default override for lists that adds the index.
		 */
		void calcElementPathHashRecursive() override {
			calcIndexedListChildPath<ListElements>({ elements }, getElementPathHash());
		}
	};
}
