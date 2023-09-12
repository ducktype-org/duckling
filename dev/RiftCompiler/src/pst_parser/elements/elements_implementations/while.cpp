#include "elements_implementation.hpp"

namespace pst {
	ParserRef<While> While::parse(RiftParserState& state) {
		// @TODO: attr list
		auto out = makeRef<While>(state.ctokens().peek().getPosition());

		RIFT_ASSERT(state.ctokens().is(Keyword::While), "bad statement choice");

		parseAll(state, Keyword::While, &out->optional_name, &out->condition, &out->body);

		return out;	
	}

	void While::dprint(std::ostream& out) const {
		out << "{\"While\": {\"name\":";
		nullAwareDprint(optional_name, out);
		out<<", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}
}
