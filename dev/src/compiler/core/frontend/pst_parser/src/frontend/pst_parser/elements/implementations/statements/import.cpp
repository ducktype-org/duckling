#include "../../hierarchy/statements/import.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Import, selectors);

	MBox<Import> Import::parse(LangParserState& state) {
		auto out = makeBox<Import>(state);

		if (!assertStmtChoice<Import>(state, state[0].is(Keyword::Import))) return nullptr;

		PARSE().all(Keyword::Import, &out->selectors);

		PST_RETURN out;
	}

	void Import::dprint(std::ostream& out) const {
		out << "{";

		out << R"("selectors": )";
		nullAwareDprint(selectors, out);

		out << "}";
	}

	HashAlg& Import::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Import::acceptVisitor(PstVisitor& visitor) const { visitor.visitImport(*this); }

	DeclKind Import::isDeclaration() const {
		if (!selectors.internal()) return DeclKind::None;
		return selectors.internal()->bindsSingleName() ? DeclKind::Symbol : DeclKind::Transparent;
	}

	base::Optional<AccessLocked<IdentifierWrapper>> Import::getDeclSymbolIdentifier() const {
		if (auto list = selectors.internal()) return list->getSingleDeclaredName();
		return {};
	}
}
