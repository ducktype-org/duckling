#include "elements_implementation.hpp"

namespace pst {
	class AliasStar final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Alias declaration cannot use `.*`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		AliasStar(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Alias> Alias::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Alias>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Alias), position.genStr("bad statement choice"));

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::Alias, &out->name, Operator::Assign, &out->points_to);

		out->setLastToken(state.getPosition(-1));

		if (out->points_to->getStar())
			state.fail(base::make_unique<AliasStar>(out->source_position));

		return out;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{\"Alias\": {";

		out << strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}}";
	}
}
