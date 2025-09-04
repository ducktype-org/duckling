#include "../../hierarchy/declarations/function.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	MBox<Fun> Fun::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Fun>(position);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		state.parse(out).all(Keyword::Fun, &out->name);
		state.parse(out).one(&out->params);

		if (state.parse(out).tryEat(NamedOperator::SingleArrow)) state.parse(out).one(&out->ret);

		// Check if next token is a code block to determine if '=' is required
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			// Code block body - no '=' required
			state.parse(out).one(&out->body);
		} else {
			// Expression body - '=' required
			state.parse(out).all(NamedOperator::Assign, &out->body);
		}

		return out;
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Fun::acceptVisitor(PstVisitor& visitor) const { visitor.visitFun(*this); }
}
