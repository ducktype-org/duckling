#pragma once

#include "../lists/selector_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Using statement: `using` followed by a `SelectorList`, e.g. `using a.b.{c, d as e};`.
	 */
	class Using final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Using, Stmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(selectors, SelectorList);

	public:
		STMT_CHILD_CONSTRUCTOR(Using, ElementKind::Using);
		static MBox<Using> parse(LangParserState& state);

		/**
		 * @brief The selectors after the `using` keyword.
		 */
		[[nodiscard]]
		AccessLocked<SelectorList> getSelectors() const {
			return selectors.give();
		}

		~Using() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Using";
		}

		/**
		 * @brief `Symbol` when the whole list binds exactly one name (`using a.b;`,
		 * `using a.b as c;`), `Transparent` otherwise.
		 */
		[[nodiscard]]
		DeclKind isDeclaration() const final;

		[[nodiscard]] base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier(
		) const final;
	};
}
