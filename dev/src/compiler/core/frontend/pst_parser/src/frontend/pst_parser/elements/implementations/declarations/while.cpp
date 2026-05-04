#include "../../hierarchy/declarations/while.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/round_group_expression.hpp"   // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<While> While::parse(LangParserState& state) {
		auto out = makeBox<While>(state);

		if (!assertStmtChoice<While>(state, state[0].is(Keyword::While))) return nullptr;

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(Keyword::While, &out->name, &out->condition, &out->body);
		})

		PST_RETURN out;
	}

	void While::dprint(std::ostream& out) const {
		out << "{";

		if (name.has_value()) {
			out << R"("name":)";
			nullAwareDprint(name.value(), out);
			out << ",";
		}
		out << "\"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);

		out << "}";
	}

	HashAlg& While::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	AccessLocked<ExprHolder> While::getCondition() const { return condition.internal()->getExpr(); }

	void While::acceptVisitor(PstVisitor& visitor) const { visitor.visitWhile(*this); }
}
