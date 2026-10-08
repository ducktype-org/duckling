// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Expression in round brackets
	 */
	class RoundExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(RoundExpr, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(expr, ExprElement);

	public:
		explicit RoundExpr(LangElementConstructionArgument state): ExprElement(state, 200) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~RoundExpr() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getInner() const {
			return expr.give();
		}
	};
}
