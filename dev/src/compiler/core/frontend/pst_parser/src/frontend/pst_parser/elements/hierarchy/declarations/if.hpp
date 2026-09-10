#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief If declaration
	 *
	 * Also covers `if const (...)`, which is evaluated at compile time and compiles only the
	 * taken branch.
	 */
	class If final: public CodeDecl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(If, CodeDecl, is_const);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(condition, RoundGroupExpr);
		NAMED_CHILD_OPT(name, IdentifierWrapper);
		NAMED_CHILD(then_body, CodeBlockOrStmt);
		NAMED_CHILD_OPT(else_body, CodeBlockOrStmt);
		bool is_const = false;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit If(const LangParserState& state): CodeDecl(state) {
			element_kind = ElementKind::If;
		}

		static MBox<If> parse(LangParserState& state);
		void            dprint(std::ostream& out) const final;
		~If() final = default;

		bool trailingSemicolon() final { return false; }

		[[nodiscard]]
		std::string elementType() const override {
			return "If";
		}

		[[nodiscard]]
		bool isConst() const {
			return is_const;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getCondition() const;

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getThenBody() const {
			return then_body.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<CodeBlockOrStmt>> getElseBody() const {
			return else_body.map([](const auto& access) { return access.give(); });
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
