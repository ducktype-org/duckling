#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Block expression
	 *
	 * A block that has value equal to the value returned from it.
	 */
	class BlockExpr final: public ExprElement {
		NAMED_CHILD(block, CodeBlock);

	public:
		explicit BlockExpr(const LangParserState& state): ExprElement(state, 200) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~BlockExpr() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		AccessLocked<CodeBlock> getBlock() { return block.give(); }

		[[nodiscard]]
		std::string elementType() const override {
			return "Block Expression";
		}
	};
}
