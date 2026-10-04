#include "../../hierarchy/not_statements/attribute.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Attribute, name, args);

	MBox<Attribute> Attribute::parse(LangParserState& state) {
		Box<Attribute> out = makeBox<Attribute>(state);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		PARSE().all(Special::AtSign, &out->name);

		// @TODO: Make a more general solution to dotted names that can't have stars
		if (out->name.internal() && out->name.internal()->getStar())
			state.logSafeError(
				makeBox<AttrStarError>(out->name.internal()->getSourcePosition().illegalAccess())
			);
		if (state[0].isBracketGroup(Token::BracketType::Round)) PARSE().one(&out->args);

		PST_RETURN out;
	}

	HashAlg& Attribute::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Attribute::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (args.internal()) {
			out << ", \"args\": ";
			nullAwareDprint(args, out);
		}
		out << "}";
	}
}
