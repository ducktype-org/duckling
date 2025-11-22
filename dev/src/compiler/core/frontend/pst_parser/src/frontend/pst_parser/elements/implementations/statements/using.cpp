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

	base::Optional<base::StrID> Using::getDeclSymbolName() const {
		std::string result = "<USING><";
		for (const auto& name: names.internal()->getNames()) result += name.value.str();
		result += ">";
		return base::StrID(result.c_str());
	}
}
