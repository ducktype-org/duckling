#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief If declaration
	 */
	class If final: public CodeDecl {
		NAMED_CHILD(condition, RoundGroupExpr);
		tpc::OptionalIdentifier optional_name;
		NAMED_CHILD(then_body, CodeBlockOrStmt);
		NAMED_CHILD_OPT(else_body, CodeBlockOrStmt);

	protected:
		HashAlg& calcStableHash(HashAlg& partial_hash) const override;

	public:
		explicit If(const dia::SourcePosition& position): CodeDecl(position) {
			element_kind = ElementKind::If;
		}

		static MBox<If> parse(LangParserState& state);
		void            dprint(std::ostream& out) const final;
		~If() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "If";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getCondition() const;

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
			return (optional_name.value ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return optional_name.value;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
