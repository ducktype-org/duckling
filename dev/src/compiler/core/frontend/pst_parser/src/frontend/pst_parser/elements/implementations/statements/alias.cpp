#include "../../hierarchy/statements/alias.hpp"

#include "../../hierarchy/not_statements/dotted_name.hpp"
#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst {
	class AliasStarError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "alias_star_error" };
		}

	public:
		AliasStarError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<Alias> Alias::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Alias>(position);

		if (!assertStmtChoice<Alias>(state, state[0].is(Keyword::Alias))) return nullptr;

		state.parse(out).all(Keyword::Alias, &out->name, NamedOperator::Assign, &out->points_to);

		if (out->points_to.internal()->getStar())
			state.logInt(makeBox<AliasStarError>(out->source_position));

		return out;
	}

	LangElement::HashAlg& Alias::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		return partial_hash;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{";

		out << strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}";
	}

	void Alias::acceptVisitor(PstVisitor& visitor) const { visitor.visitAlias(*this); }
}
