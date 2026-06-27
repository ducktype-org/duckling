#include "../../hierarchy/class_elements/copy_constructor.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(CopyConstructor, params, inits, body)

	MBox<CopyConstructor> CopyConstructor::parse(LangParserState& state) {
		auto out = makeBox<CopyConstructor>(state);

		CORE_ASSERT(
			state[0].is(state.getContext()->class_name), "Bad copy constructor parsing entry"
		);

		PARSE().eatOne();

		PARSE().all(NamedOperator::Period, Keyword::Copy);

		PARSE().one(&out->params);
		if (PARSE().tryEat(NamedOperator::Colon)) PARSE().one(&out->inits);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			state.setContextStmt(StmtContext::Normal);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	void CopyConstructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		out << "\"" << getInternalSymbolName()->str() << "\"";
		out << ",\"params\":";
		nullAwareDprint(params, out);
		out << ",\"inits\":";
		nullAwareDprint(inits, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void CopyConstructor::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitCopyConstructor(*this);
	}
}
