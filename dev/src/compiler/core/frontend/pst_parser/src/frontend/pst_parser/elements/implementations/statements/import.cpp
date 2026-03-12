#include "../../hierarchy/statements/import.hpp"

#include "../../hierarchy/not_statements/import_chain.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Import> Import::parse(LangParserState& state) {
		auto out = makeBox<Import>(state);

		if (!assertStmtChoice<Import>(state, state[0].is(Keyword::Import))) return nullptr;

		PARSE().all(Keyword::Import, &out->import_chain);

		state.addImport(out.ref());
		PST_RETURN out;
	}

	void Import::dprint(std::ostream& out) const {
		out << "{";

		out << R"("import_chain": )";
		nullAwareDprint(import_chain, out);

		out << "}";
	}

	HashAlg& Import::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Import::acceptVisitor(PstVisitor& visitor) const { visitor.visitImport(*this); }
}
