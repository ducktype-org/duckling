
#include "../../../hierarchy/not_statements/patterns/wildcard_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<WildcardPattern> WildcardPattern::parse(LangParserState& state) {
		if (!state[0].is(Special::Underscore)) return nullptr;

		auto position = state.getPosition();
		auto out      = makeBox<WildcardPattern>(position);
		state.parse(out).eatOne();
		return out;
	}

	void WildcardPattern::dprint(std::ostream& out) const {
		out << R"({ "pattern_type": "wildcard" })";
	}

	LangElement::HashAlg& WildcardPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void WildcardPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitWildcardPattern(*this);
	}
}
