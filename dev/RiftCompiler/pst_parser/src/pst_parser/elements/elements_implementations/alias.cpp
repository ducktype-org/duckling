#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Alias> Alias::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Alias>(position);

		RIFT_ASSERT(
			state.ctokens().is(Keyword::Alias), position.genErrorStr("bad statement choice")
		);

		parseAll(state, Keyword::Alias, &out->name, Operator::Assign, &out->points_to);

		if (out->points_to->getStar()) {
			state.err.failAndLog(
				state.ctokens().peek(-1).getPosition(), "Alias declaration can not have `.*`"
			);
		}

		return out;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{\"Alias\": {";

		out << strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}}";
	}
}
