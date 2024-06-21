#include "elements_implementation.hpp"

namespace pst {
	ParserRef<While> While::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<While>(position);

		if (!assertStmtChoice<While>(state, state[0].is(Keyword::While))) return nullptr;

		state.parse().all(Keyword::While, &out->optional_name, &out->condition, &out->body);

		out->setLastToken(state.getPosition(-1));

		return out;
	}

	void While::dprint(std::ostream& out) const {
		out << R"({"While": {"name":)";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}
}
