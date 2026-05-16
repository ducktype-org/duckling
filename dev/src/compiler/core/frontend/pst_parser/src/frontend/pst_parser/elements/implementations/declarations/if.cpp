#include "../../hierarchy/declarations/if.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/round_group_expression.hpp"   // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<If> If::parse(LangParserState& state) {
		auto out = makeBox<If>(state);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(Keyword::If, &out->name, &out->condition, &out->then_body);

			if (PARSE().tryEat(Keyword::Else)) PARSE().one(&out->else_body);
		})

		PST_RETURN out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{";

		if (name.has_value()) {
			out << "\"name\":";
			nullAwareDprint(name.value(), out);
			out << ",";
		}
		out << "\"condition\":";
		nullAwareDprint(condition, out);
		out << ",\"then body\":";
		nullAwareDprint(then_body, out);
		if (else_body) {
			out << ",\"else body\":";
			nullAwareDprint(else_body.value(), out);
		}

		out << "}";
	}

	HashAlg& If::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, else_body.has_value());
		return partial_hash;
	}

	AccessLocked<ExprHolder> If::getCondition() const { return condition.internal()->getExpr(); }

	void If::acceptVisitor(PstVisitor& visitor) const { visitor.visitIf(*this); }
}
