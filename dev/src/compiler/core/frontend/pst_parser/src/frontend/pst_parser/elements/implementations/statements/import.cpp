#include "../../hierarchy/statements/import.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<Import> Import::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Import>(position);

		if (!assertStmtChoice<Import>(state, state[0].is(Keyword::Import))) return nullptr;

		state.parse(out).all(Keyword::Import, &out->names, Keyword::As, &out->alias);

		state.addImport(out.ref());
		PST_RETURN out;
	}

	const decltype(Import::names)& Import::getNames() const { return names; }

	std::vector<base::StrID> Import::getModulePath() const {
		std::vector<base::StrID> path;
		for (auto& elem: *names.internal()) path.emplace_back(elem.value);
		return path;
	}

	bool Import::getStar() const { return names.internal()->getStar(); }

	void Import::dprint(std::ostream& out) const {
		out << "{\"Import\": ";
		nullAwareDprint(names, out);
		out << ", ";
		out << R"("Alias": ")" << alias.value.strView() << R"(")";
		out << "}";
	}

	LangElement::HashAlg& Import::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, alias);
		return partial_hash;
	}

	void Import::acceptVisitor(PstVisitor& visitor) const { visitor.visitImport(*this); }
}
