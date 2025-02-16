#include "preamble.hpp"

namespace pst {
	MBox<ClassBlock> ClassBlock::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<ClassBlock>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		while (state.notEmpty()) {
			MBox<ClassStmt> stmt;
			state.parse(out).with(&stmt, ClassStmt::parse, ctx);
			out->statements.emplace_back(std::move(stmt));
		}

		state.parse(out).goUpAndSkip();
		return out;
	}

	void ClassBlock::dprint(std::ostream& out) const {
		out << "[";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]";
	}
}
