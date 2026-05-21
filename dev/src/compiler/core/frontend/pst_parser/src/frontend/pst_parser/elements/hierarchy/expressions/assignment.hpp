#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
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
		NAMED_CHILD(type, OperatorWrapper);
		NAMED_CHILD(value, ExprElement);

	public:
		explicit Assignment(const LangParserState& state): ExprElement(state, 1'000) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Assignment Expr";
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getVariables() const {
			return variables.give();
		}

		[[nodiscard]]
		AccessLocked<OperatorWrapper> getAssignmentType() const {
			return type.give();
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getValue() const {
			return value.give();
		}

		static MBox<ExprElement> parse(LangParserState& state);

		~Assignment() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
