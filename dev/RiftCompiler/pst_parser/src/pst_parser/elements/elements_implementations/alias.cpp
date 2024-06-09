#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	class AliasStarError final: public dia::Error {
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

		AliasStarError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Alias> Alias::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Alias>(position);

		if (!assertStmtChoice<Alias>(state, state[0].is(Keyword::Alias))) return nullptr;

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::Alias, &out->name, Operator::Assign, &out->points_to);

		out->setLastToken(state.getPosition(-1));

		if (out->points_to->getStar())
			state.fail(base::make_unique<AliasStarError>(out->source_position));

		return out;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{\"Alias\": {";

		out << strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}}";
	}

	void Alias::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitAlias(*this); }
}
