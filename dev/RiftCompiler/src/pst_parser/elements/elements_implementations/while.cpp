#include "elements_implementation.hpp"

namespace pst {
	ParserRef<While> While::parse(RiftParserState &state) {
		// @TODO: attr list
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<While>(position);

		RIFT_ASSERT(
			state.ctokens().is(Keyword::While), position.genErrorMsg("bad statement choice")
		);

		parseAll(state, Keyword::While, &out->optional_name, &out->condition, &out->body);

		return out;
	}

	void While::dprint(std::ostream &out) const {
		out << R"({"While": {"name":)";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}
}
