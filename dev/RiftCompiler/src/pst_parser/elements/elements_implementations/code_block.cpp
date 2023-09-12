#include "elements_implementation.hpp"

namespace pst {
	ParserRef<CodeBlock> CodeBlock::parse(RiftParserState& state) {

		if (!state.ctokens().is(Token::Type::CurlyGroup)) {
			state.fail(-1, "expected `{` after here");
			return nullptr;
		}

		auto out = makeRef<CodeBlock>(state.ctokens().peek().getPosition());

		state.goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) {
			out->statements.emplace_back(Stmt::parse(state));
		}

		state.goUpAndSkip();
		return out;
	}

	void CodeBlock::dprint(std::ostream& out) const {
		out << "{\"CodeBlock\": [";
		for(auto &stmt: statements) {
			nullAwareDprint(stmt, out);
			out<<", ";
		}
		out<<"]}";
	}
}
