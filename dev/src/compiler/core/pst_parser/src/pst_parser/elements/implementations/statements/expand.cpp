#include "../../hierarchy/statements/expand.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<Expand> Expand::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Expand>(position);

		if (!assertStmtChoice<Expand>(state, state[0].is(Keyword::Expand))) return nullptr;

		state.parse(out).all(Keyword::Expand, &out->value);

		return out;
	}

	/**
	 * @note For now this will break on escape characters.
	 */
	void Expand::dprint(std::ostream& out) const {
		out << "{";

		out << R"("value": ")";
		nullAwareDprint(value, out);
		out << R"(",)";

		out << "}";
	}

	LangElement::HashAlg& Expand::calcStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Expand::acceptVisitor(PstVisitor& visitor) const { visitor.visitExpand(*this); }
}
