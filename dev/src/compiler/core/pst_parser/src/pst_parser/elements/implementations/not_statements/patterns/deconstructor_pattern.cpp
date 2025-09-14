#include "../../../hierarchy/not_statements/patterns/deconstructor_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<DeconstructorPattern> DeconstructorPattern::parse(LangParserState& state) {
		if (!state[0].isIdentifier() || !state[1].isBracketGroup(lexer::Token::BracketType::Round))
			return nullptr;

		auto position = state.getPosition();
		auto out      = makeBox<DeconstructorPattern>(position);

		state.parse(out).one(&out->deconstructor_name);
		state.parse(out).one(&out->arguments);
		return out;
	}

	void DeconstructorPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "deconstructor",)";
		out << R"("name": )";
		nullAwareDprint(deconstructor_name, out);
		out << R"(, "arguments": [)";
		nullAwareDprint(arguments, out);
		out << "]}";
	}

	void DeconstructorPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitDeconstructorPattern(*this);
	}
}
