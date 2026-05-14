#include "../../hierarchy/expressions/block_expr.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> BlockExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());
		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Curly))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadBlockError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<BlockExpr>(state);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().one(&out->block);
		})

		PST_RETURN out;
	}

	void BlockExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	HashAlg& BlockExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void BlockExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitBlockExpr(*this);
	}
}
