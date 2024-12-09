#include "preamble.hpp"

namespace pst {
	MBox<Import> Import::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Import>(position);

		if (!assertStmtChoice<Import>(state, state[0].is(Keyword::Import))) return nullptr;

		state.parse(out).all(Keyword::Import, &out->names, Keyword::As, &out->alias);

		state.addImport(out.ref());
		return out;
	}

	const decltype(Import::names)& Import::getNames() const { return names; }

	std::vector<base::StrID> Import::getModulePath() const {
		std::vector<base::StrID> out;
		for (auto& elem: *names) out.emplace_back(elem.value);
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

	void Import::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitImport(*this); }
}
