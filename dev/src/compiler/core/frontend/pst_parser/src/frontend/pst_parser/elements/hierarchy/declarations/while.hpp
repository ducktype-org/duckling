#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief While declaration
	 */
	class While final: public CodeDecl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(While, CodeDecl);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(condition, RoundGroupExpr);
		NAMED_CHILD_OPT(name, IdentifierWrapper);
		NAMED_CHILD(body, CodeBlockOrStmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit While(LangElementConstructionArgument state): CodeDecl(state) {
			element_kind = ElementKind::While;
		}

		static MBox<While> parse(LangParserState& state);
		void               dprint(std::ostream& out) const final;
		~While() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "While";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getCondition() const;

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return (name.has_value() ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return name.map([](const auto& acc) { return acc.give(); });
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
