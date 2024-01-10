#include "elements_implementation.hpp"

namespace pst {
	ParserRef<CodeBlock> CodeBlock::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		if (!state.ctokens().isBracketGroup(Token::BracketType::Curly)) {
			state.fail(-1, "expected `{` after here");
			return nullptr;
		}


		auto out = makeRef<CodeBlock>(position);
		state.goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) out->statements.emplace_back(Stmt::parse(state));

		state.goUpAndSkip();
		return out;
	}

	void CodeBlock::dprint(std::ostream& out) const {
		out << "{\"CodeBlock\": [";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]}";
	}
}
