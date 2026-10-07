// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Block expression
	 *
	 * A block that has value equal to the value returned from it.
	 */
	class BlockExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(BlockExpr, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(block, CodeBlock);

	public:
		explicit BlockExpr(const LangParserState& state): ExprElement(state, 200) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~BlockExpr() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]] AccessLocked<CodeBlock> getBlock() const { return block.give(); }

		[[nodiscard]]
		std::string elementType() const override {
			return "Block Expression";
		}
	};
}
