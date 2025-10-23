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
		explicit BlockExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

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
