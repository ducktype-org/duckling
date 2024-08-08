#include "preamble.hpp"

namespace pst {
	ParserRef<ClassBlock> ClassBlock::parse(RiftParserState& state, tpc::Identifier class_name) {
		auto position = state.getPosition();
		auto out      = makeRef<ClassBlock>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(base::make_unique<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) {
			ParserRef<ClassStmt> stmt;
			state.parse(out).with(&stmt, ClassStmt::parse, tpc::Identifier(class_name));
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
