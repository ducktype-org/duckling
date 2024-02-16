#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Alias> Alias::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Alias>(position);

		RIFT_ASSERT(
			state.ctokens().is(Keyword::Alias), position.genErrorStr("bad statement choice")
		);

		parseAll(state, Keyword::Alias, &out->name);
		parseOne(state, Operator::Assign);
		parseDottedName(state, &out->points_to);

		if (out->points_to.star) {
			out->points_to.star = false;
			state.err.setFail();
			state.err.logError(
				state.ctokens().peek(-1).getPosition(), "Alias declaration can not have `.*`"
			);
		}

		return out;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{\"Alias\": {";

		out << base::strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": [)";

		for (const auto& sub_name: points_to.names) {
			tpc::nullAwareDprint(sub_name, out);
			out << ", ";
		}

		out << "]}}";
	}
}
