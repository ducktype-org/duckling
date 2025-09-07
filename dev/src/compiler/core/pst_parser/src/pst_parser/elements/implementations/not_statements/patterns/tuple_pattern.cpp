#include "../../../hierarchy/not_statements/patterns/tuple_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<TuplePattern> TuplePattern::parse(LangParserState& state) {
		if (!state[0].isBracketGroup(lexer::Token::BracketType::Round)) return nullptr;
		auto position = state.getPosition();
		auto out      = makeBox<TuplePattern>(position);
		state.parse(out).one(&out->elements);
		return out;
	}

	void TuplePattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "tuple",)";
		out << R"("elements": [)";
		nullAwareDprint(elements, out);
		out << "]}";
	}

	void TuplePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitTuplePattern(*this);
	}
}
