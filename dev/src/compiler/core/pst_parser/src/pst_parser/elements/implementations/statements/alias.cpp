#include "../../hierarchy/statements/alias.hpp"

#include "../../hierarchy/not_statements/dotted_name.hpp"
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

	MBox<Alias> Alias::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Alias>(position);

		if (!assertStmtChoice<Alias>(state, state[0].is(Keyword::Alias))) return nullptr;

		state.parse(out).all(Keyword::Alias, &out->name, NamedOperator::Assign, &out->points_to);

		if (out->points_to.internal()->getStar())
			state.log(makeBox<AliasStarError>(out->source_position));

		return out;
	}

	u64 Alias::calcStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		return partial_hash.finalize();
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
