// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../lists/selector_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Import statement: `import` followed by a `SelectorList`, e.g. `import a.b.*;`.
	 */
	class Import final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Import, Stmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(selectors, SelectorList);

	public:
		STMT_CHILD_CONSTRUCTOR(Import, ElementKind::Import);
		static MBox<Import> parse(LangParserState& state);

		~Import() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		/**
		 * @brief The selectors after the `import` keyword.
		 */
		[[nodiscard]]
		AccessLocked<SelectorList> getSelectors() const {
			return selectors.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import";
		}

		/**
		 * @brief `Symbol` when the whole list binds exactly one name (`import a.b;`,
		 * `import a.b as c;`), `Transparent` otherwise.
		 */
		[[nodiscard]]
		DeclKind isDeclaration() const final;

		[[nodiscard]] base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier(
		) const final;
	};
}
