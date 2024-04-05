#include "elements_implementation.hpp"
#include "token_parser_core/automatic.hpp"

namespace pst {
	ParserRef<Import> Import::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Import>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Import), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Import, &out->names);

		state.addImport(out.borrow());

		return out;
	}

	const decltype(Import::names)& Import::getNames() const { return names; }

	bool Import::getStar() const { return names->getStar(); }

	void Import::dprint(std::ostream& out) const {
		out << "{\"Import\": ";
		nullAwareDprint(names, out);
		out << "}";
	}
}
