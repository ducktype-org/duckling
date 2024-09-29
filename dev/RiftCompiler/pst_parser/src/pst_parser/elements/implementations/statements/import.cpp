#include "preamble.hpp"

namespace pst {
	ParserRef<Import> Import::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Import>(position);

		if (!assertStmtChoice<Import>(state, state[0].is(Keyword::Import))) return nullptr;

		state.parse(out).all(Keyword::Import, &out->names, Keyword::As, &out->alias);

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

	void Import::semPrint(std::ostream& out) const {
		out << "{\"Import\": {";
		getSourcePosition().semPrint(out);
		out << R"(,"foldingRangeKind": "imports",)";  // @TODO: maybe needs a change?
		nullAwareSemanticTokenPrint(names, out);
		out << ", ";
		out << R"("Alias": ")" << alias.value.strView() << R"(")";
		out << "}}";
	}

	void Import::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitImport(*this); }
}
