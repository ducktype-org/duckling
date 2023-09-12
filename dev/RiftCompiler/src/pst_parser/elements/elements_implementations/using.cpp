#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Using> Using::parse(RiftParserState& state) {
		auto out = makeRef<Using>(state.ctokens().peek().getPosition());

		RIFT_ASSERT(state.ctokens().is(Keyword::Using), "bad statement choice");

		parseOne(state, Keyword::Using);
		parseDottedName(state, &out->names);

		return out;
	}

	
	void Using::dprint(std::ostream& out) const {
		out << "{\"Using\": {";

		if(names.star)
			out << R"("star": "true",)";
		else
			out << R"("star": "false",)";
		
		out << R"("names": [)";

		for (const auto& name: names.names){
			tpc::nullAwareDprint(name, out);
			out<<", ";
		}

		out << "]}}";
	}
}
