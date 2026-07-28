#include "../../hierarchy/class_elements/move_constructor.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(MoveConstructor, params, body)

	MBox<MoveConstructor> MoveConstructor::parse(LangParserState& state) {
		auto out = makeBox<MoveConstructor>(state);

		CORE_ASSERT(
			state[0].is(state.getContext()->class_name), "Bad move constructor parsing entry"
		);

		PARSE().eatOne();

		PARSE().all(NamedOperator::Period, Keyword::Move);

		PARSE().one(&out->params);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			state.setContextStmt(StmtContext::Normal);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	void MoveConstructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		out << "\"" << getInternalSymbolName()->str() << "\"";
		out << ",\"params\":";
		nullAwareDprint(params, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void MoveConstructor::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitMoveConstructor(*this);
	}
}
