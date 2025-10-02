#include "../../hierarchy/declarations/if.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/round_group_expression.hpp"   // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<If> If::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<If>(position);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		state.parse(out)
			.all(Keyword::If, &out->optional_name, &out->condition)
			.withDef(&out->then_body, CodeBlock::CodeBlockType::Ordered);
		;

		if (state.parse(out).tryEat(Keyword::Else))
			state.parse(out).withDef(&out->else_body, CodeBlock::CodeBlockType::Ordered);

		return out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{\"name\":";
		nullAwareDprint(optional_name, out);
		out << ",\"condition\":";
		nullAwareDprint(condition, out);
		out << ",\"then body\":";
		nullAwareDprint(then_body, out);
		if (else_body) {
			out << ",\"else body\":";
			nullAwareDprint(else_body.value(), out);
		}
		out << "}";
	}

	LangElement::HashAlg& If::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, optional_name);
		addToHash(partial_hash, else_body.has_value());
		return partial_hash;
	}

	AccessLocked<ExprHolder> If::getCondition() const { return condition.internal()->getExpr(); }

	void If::acceptVisitor(PstVisitor& visitor) const { visitor.visitIf(*this); }
}
