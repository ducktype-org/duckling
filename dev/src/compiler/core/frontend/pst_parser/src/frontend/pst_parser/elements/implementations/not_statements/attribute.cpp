#include "../../hierarchy/not_statements/attribute.hpp"

#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst {
	class AttrStarError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "attr_star_error" };
		}

	public:
		AttrStarError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<Attribute> Attribute::parse(LangParserState& state) {
		auto           position = state.getPosition();
		Box<Attribute> out      = makeBox<Attribute>(position);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		state.parse(out).all(Special::AtSign, &out->name);

		// @TODO: Make a more general solution to dotted names that can't have stars
		if (out->name.internal() && out->name.internal()->getStar())
			state.logInt(makeBox<AttrStarError>(out->name.internal()->getSourcePosition()));
		if (state[0].isBracketGroup(Token::BracketType::Round)) state.parse(out).one(&out->args);

		return out;
	}

	LangElement::HashAlg& Attribute::addElementDataToStableHash(HashAlg& partial_hash) const {
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
