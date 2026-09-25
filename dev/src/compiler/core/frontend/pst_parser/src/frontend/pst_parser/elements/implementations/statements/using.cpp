#include "../../hierarchy/statements/using.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Using, selectors);

	MBox<Using> Using::parse(LangParserState& state) {
		auto out = makeBox<Using>(state);

		if (!assertStmtChoice<Using>(state, state[0].is(Keyword::Using))) return nullptr;

		PARSE().all(Keyword::Using, &out->selectors);

		PST_RETURN out;
	}

	void Using::dprint(std::ostream& out) const {
		out << "{";

		out << R"("selectors": )";
		nullAwareDprint(selectors, out);

		out << "}";
	}

	HashAlg& Using::addElementDataToStableHash(HashAlg& partial_hash) const { return partial_hash; }

	void Using::acceptVisitor(PstVisitor& visitor) const { visitor.visitUsing(*this); }

	DeclKind Using::isDeclaration() const {
		if (!selectors.internal()) return DeclKind::None;
		return selectors.internal()->bindsSingleName() ? DeclKind::Symbol : DeclKind::Transparent;
	}

	base::Optional<AccessLocked<IdentifierWrapper>> Using::getDeclSymbolIdentifier() const {
		if (auto list = selectors.internal()) return list->getSingleDeclaredName();
		return {};
	}
}
