// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Ternary expression(`if condition then if_true else if_else`).
	 *
	 * @note For now the parsing of ternary is pretty limited with only one such expression
	 * without any parenthesis. This is a limited but safe option.
	 *
	 * @note This should be the default starting level for an expression when comma expression
	 * would cause parsing problems.
	 */
	class Ternary final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Ternary, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		using Lower = LogicOr;

		NAMED_CHILD(condition, ExprElement);
		NAMED_CHILD(if_true, ExprElement);
		NAMED_CHILD(if_false, ExprElement);

	public:
		explicit Ternary(LangElementConstructionArgument state): ExprElement(state, 800) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Ternary Expr";
		}

		static MBox<ExprElement> parse(LangParserState& state);

		~Ternary() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<ExprElement> getCondition() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getIfTrue() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getIfFalse() const;
	};
}
