#include "../../hierarchy/statements/alias.hpp"

#include "../../hierarchy/not_statements/dotted_name.hpp"
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Alias, name, points_to);

	MBox<Alias> Alias::parse(LangParserState& state) {
		auto out = makeBox<Alias>(state);

		if (!assertStmtChoice<Alias>(state, state[0].is(Keyword::Alias))) return nullptr;

		PARSE().all(Keyword::Alias, &out->name, NamedOperator::Assign, &out->points_to);

		if (out->points_to.internal()->getStar())
			state.logInt(makeBox<AliasStarError>(out->source_position));

		PST_RETURN out;
	}

	HashAlg& Alias::addElementDataToStableHash(HashAlg& partial_hash) const { return partial_hash; }

	void Alias::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(,)";

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}";
	}

	void Alias::acceptVisitor(PstVisitor& visitor) const { visitor.visitAlias(*this); }
}
