#include "preamble.hpp"

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

		state.parse(out).all(Keyword::Alias, &out->name, Operator::Assign, &out->points_to);

		if (out->points_to->getStar())
			state.log(base::make_unique<AliasStarError>(out->source_position));

		return out;
	}

	void Alias::dprint(std::ostream& out) const {
		out << "{";

		out << strConcat(R"("name": ")", name.value, R"(",)");

		out << R"("points_to": )";

		nullAwareDprint(points_to, out);

		out << "}";
	}

	void Alias::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitAlias(*this); }
}
