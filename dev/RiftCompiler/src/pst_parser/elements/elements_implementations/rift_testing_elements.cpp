#include "elements_implementation.hpp"

namespace pst {
	ParserRef<EagerLookup> EagerLookup::parse(RiftParserState& state) {
		auto out = makeRef<EagerLookup>();
		RIFT_ASSERT(state.ctokens().is(Keyword::RiftTestEagerLookup), "bad statement choice");

		parseAll(state, Keyword::RiftTestEagerLookup);
		parseDottedName(state, &out->names);

		return out;
	}

	void EagerLookup::dprint(std::ostream& out) const {
		out << "{\"EagerLookup\": {";

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