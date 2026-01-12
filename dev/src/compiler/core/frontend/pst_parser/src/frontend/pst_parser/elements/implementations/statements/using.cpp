#include "../../hierarchy/statements/using.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<Using> Using::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Using>(position);

		if (!assertStmtChoice<Using>(state, state[0].is(Keyword::Using))) return nullptr;

		state.parse(out).all(Keyword::Using, &out->names);

		return out;
	}

	void Using::dprint(std::ostream& out) const { nullAwareDprint(names, out); }

	LangElement::HashAlg& Using::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Using::acceptVisitor(PstVisitor& visitor) const { visitor.visitUsing(*this); }

	DeclKind Using::isDeclaration() const {
		if (!names.internal()) return DeclKind::None;
		return names.internal()->getStar() ? DeclKind::Transparent : DeclKind::Symbol;
	}

	base::Optional<base::StrID> Using::getDeclSymbolName() const {
		return names.internal().toOpt().map([](const auto& x) { return x->getNames().back().value; }
		);
	}
}
