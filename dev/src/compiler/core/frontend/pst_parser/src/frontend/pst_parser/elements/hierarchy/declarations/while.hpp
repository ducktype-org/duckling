#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief While declaration
	 */
	class While final: public CodeDecl {
		NAMED_CHILD(condition, RoundGroupExpr);
		tpc::OptionalIdentifier optional_name;
		NAMED_CHILD(body, CodeBlockOrStmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit While(const LangParserState& state): CodeDecl(state) {
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
			return (optional_name.value ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return optional_name.value;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
