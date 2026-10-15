// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for suffix operator elements.
	 */
	class SuffixOperator: public ExprElement {
		THIS_CLASS(SuffixOperator);
		PARENT_CLASS(ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(op, OperatorWrapper);
		NAMED_CHILD(expr, ExprElement);

	public:
		ELEMENT_CLONE_DECL(SuffixOperator);

		explicit SuffixOperator(LangElementConstructionArgument state, i64 precedence):
			  ExprElement(state, precedence) {}

		~SuffixOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Suffix Operator";
		}

		[[nodiscard]]
		AccessLocked<OperatorWrapper> getOperator() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const;
	};
}
