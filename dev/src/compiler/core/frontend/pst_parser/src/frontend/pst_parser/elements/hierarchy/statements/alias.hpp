#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Alias statement.
	 */
	class Alias final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Alias, Stmt);
		CLONE_SUBELEMENTS();
	protected:

		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(points_to, DottedName);

	public:
		STMT_CHILD_CONSTRUCTOR(Alias, ElementKind::Alias);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<DottedName> getPointed() const {
			return points_to.give();
		}

		static MBox<Alias> parse(LangParserState& state);
		~Alias() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Alias";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}
	};
}
