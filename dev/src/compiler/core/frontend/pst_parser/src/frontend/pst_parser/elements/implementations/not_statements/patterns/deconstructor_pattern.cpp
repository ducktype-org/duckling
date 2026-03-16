#include "../../../hierarchy/not_statements/patterns/deconstructor_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<DeconstructorPattern> DeconstructorPattern::parse(LangParserState& state) {
		if (!state[0].isIdentifier() || !state[1].isBracketGroup(lexer::Token::BracketType::Round))
			return nullptr;

		auto out = makeBox<DeconstructorPattern>(state);

		PARSE().one(&out->deconstructor_name);
		PARSE().one(&out->arguments);
		PST_RETURN out;
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

	HashAlg& DeconstructorPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, deconstructor_name);
		return partial_hash;
	}

	void DeconstructorPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitDeconstructorPattern(*this);
	}
}
