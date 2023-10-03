#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Import> Import::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Import>(position);

		RIFT_ASSERT(
			state.ctokens().is(Keyword::Import), position.genErrorMsg("bad statement choice")
		);

		parseOne(state, Keyword::Import);
		parseDottedName(state, &out->names);

		state.addImport(out.borrow());

		return out;
	}

	const decltype(Import::names)& Import::getNames() const { return names; }

	bool Import::getStar() const { return names.star; }

	void Import::dprint(std::ostream& out) const {
		out << "{\"Import\": {";

		if (names.star)
			out << R"("star": "true",)";
		else
			out << R"("star": "false",)";

		out << R"("names": [)";

		for (const auto& name: names.names) {
			tpc::nullAwareDprint(name, out);
			out << ", ";
		}

		out << "]}}";
	}
}
