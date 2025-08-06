#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief If declaration
	 */
	class If final: public CodeDecl {
		AccessInternal<RoundGroupExpr>  condition;
		tpc::OptionalIdentifier         optional_name;
		AccessInternal<CodeBlockOrStmt> then_body;
		AccessInternal<CodeBlockOrStmt> else_body;

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
		bool hasElseBody() const {
			return else_body.internal();
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getElseBody() const {
			return else_body.give();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
