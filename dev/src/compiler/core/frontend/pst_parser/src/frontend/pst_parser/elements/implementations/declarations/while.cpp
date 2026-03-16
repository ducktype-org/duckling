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
			PARSE().all(Keyword::While, &out->optional_name, &out->condition, &out->body);
		})

		PST_RETURN out;
	}

	void While::dprint(std::ostream& out) const {
		out << R"({"name":)";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}";
	}

	HashAlg& While::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, optional_name);
		return partial_hash;
	}

	AccessLocked<ExprHolder> While::getCondition() const { return condition.internal()->getExpr(); }

	void While::acceptVisitor(PstVisitor& visitor) const { visitor.visitWhile(*this); }
}
