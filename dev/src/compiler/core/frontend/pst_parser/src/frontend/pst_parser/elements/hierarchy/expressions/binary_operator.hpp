// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for binary operator elements.
	 */
	class BinaryOperator: public ExprElement {
		THIS_CLASS(BinaryOperator);
		PARENT_CLASS(ExprElement);

	protected:
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(left, ExprElement);
		NAMED_CHILD(op, OperatorWrapper);
		NAMED_CHILD(right, ExprElement);

	public:
		ELEMENT_CLONE_DECL(BinaryOperator);

		explicit BinaryOperator(LangElementConstructionArgument state, i64 precedence):
			  ExprElement(state, precedence) {}

		~BinaryOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<ExprElement> getLeftOperand() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getRightOperand() const;
		[[nodiscard]]
		AccessLocked<OperatorWrapper> getOperator() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Binary Operator";
		}
	};
}
