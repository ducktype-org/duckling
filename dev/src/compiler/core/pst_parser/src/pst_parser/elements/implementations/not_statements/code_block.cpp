#include "../../hierarchy/not_statements/code_block.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlock> CodeBlock::parse(LangParserState& state, CodeBlockType order_type) {
		CORE_ASSERT(order_type != Undefined, "Parsing with an undefined ordering type");

		auto position = state.getPosition();
		auto out      = makeBox<CodeBlock>(position);

		out->type = order_type;

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			out->statements.emplace_back(nullptr);
			state.parse(out).assign(&out->statements.back(), std::move(stmt));
		}

		state.parse(out).goUpAndSkip();
		return out;
	}

	void CodeBlock::dprint(std::ostream& out) const {
		out << "[";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]";
	}
}
