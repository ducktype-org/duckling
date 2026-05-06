#include "../../hierarchy/class_elements/copy_constructor.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<CopyConstructor> CopyConstructor::parse(LangParserState& state) {
		auto out = makeBox<CopyConstructor>(state);

		PARSE().eatOne();

		out->key.emplace();
		PARSE().all(NamedOperator::Period, &out->key.value());

		PARSE().one(&out->params);
		if (PARSE().tryEat(NamedOperator::Colon)) PARSE().one(&out->inits);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
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
