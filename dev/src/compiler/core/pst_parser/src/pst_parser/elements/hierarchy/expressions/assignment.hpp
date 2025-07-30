#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This is an assignment expression.
	 *
	 * @note An assignment expression is supposed to appear only once in a stmt expression.
	 */
	class Assignment final: public ExprElement {
		using Lower = Comma;

		NAMED_CHILD(variables, ExprElement);
		base::StrID                 type;
		NAMED_CHILD(value, ExprElement);

	public:
		explicit Assignment(const dia::SourcePosition& position): ExprElement(position, 1'000) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Assignment Expr";
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getVariables() const {
			return variables.give();
		}

		[[nodiscard]]
		base::StrID getAssignmentType() const {
			return type;
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getValue() const {
			return value.give();
		}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~Assignment() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;
	};
}
