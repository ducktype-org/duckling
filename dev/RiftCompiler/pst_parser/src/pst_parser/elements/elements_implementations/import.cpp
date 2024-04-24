#include "elements_implementation.hpp"
#include "token_parser_core/automatic.hpp"

namespace pst {
	ParserRef<Import> Import::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Import>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Import), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Import, &out->names, Keyword::As, &out->alias);

		state.addImport(out.borrow());

		return out;
	}

	const decltype(Import::names)& Import::getNames() const { return names; }

	std::vector<base::StrId> Import::getModulePath() const {
		std::vector<base::StrId> out;
		for (auto& elem: names) out.emplace_back(elem.value);
		return out;
	}

	bool Import::getStar() const { return names->getStar(); }

	void Import::dprint(std::ostream& out) const {
		out << "{\"Import\": ";
		nullAwareDprint(names, out);
		out << ", ";
		out << R"("Alias": ")" << alias.value.strView() << R"(")";
		out << "}";
	}
}
