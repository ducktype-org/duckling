#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Statement that is an expression.
	 */
	class ExprStmt final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprStmt, Stmt);
		CLONE_SUBELEMENTS();
	protected:

		NAMED_CHILD(expr, AssignmentExprHolder);

	public:
		explicit ExprStmt(const LangParserState& state): Stmt(StmtKind::ExprStmt, state) {
			this->element_kind = ElementKind::ExprStmt;
		}

		static MBox<ExprStmt> parse(LangParserState& state);

		~ExprStmt() override = default;
		void     dprint(std::ostream& out) const override;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Expr Stmt";
		}

		[[nodiscard]]
		AccessLocked<AssignmentExprHolder> getExpr() const {
			return expr.give();
		}

		void acceptVisitor(PstVisitor&) const override;
	};
}
