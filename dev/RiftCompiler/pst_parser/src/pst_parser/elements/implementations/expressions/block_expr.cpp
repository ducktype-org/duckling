#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	ParserRef<ExprElement> BlockExpr::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Round Group Expression";
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Curly))) {}  // Error

		auto out = base::make_unique<BlockExpr>(state.getPosition());

		state.parse(out).one(&out->block);

		return out;
	}

	void BlockExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("block": )";
		nullAwareDprint(block, out);

		out << "}";
	}
}
