#include "../../hierarchy/statements/using.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Using, names);

	MBox<Using> Using::parse(LangParserState& state) {
		auto out = makeBox<Using>(state);

		if (!assertStmtChoice<Using>(state, state[0].is(Keyword::Using))) return nullptr;

		PARSE().all(Keyword::Using, &out->names);

		PST_RETURN out;
	}

	void Using::dprint(std::ostream& out) const { nullAwareDprint(names, out); }

	HashAlg& Using::addElementDataToStableHash(HashAlg& partial_hash) const { return partial_hash; }

	void Using::acceptVisitor(PstVisitor& visitor) const { visitor.visitUsing(*this); }

	DeclKind Using::isDeclaration() const {
		if (!names.internal()) return DeclKind::None;
		return names.internal()->getStar() ? DeclKind::Transparent : DeclKind::Symbol;
	}

	base::Optional<AccessLocked<IdentifierWrapper>> Using::getDeclSymbolIdentifier() const {
		if (auto child = names.internal()) {
			if (child->numberOfNames() == 0) return {};
			return child->getNameByIndex(child->numberOfNames() - 1);
		} else {
			return {};
		}
	}
}
