#include "../../hierarchy/class_elements/copy_constructor.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<CopyConstructor> CopyConstructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<CopyConstructor>(position, ctx);

		PARSE().eatOne();

		out->kind = Keyword::Copy;
		PARSE().all(NamedOperator::Period, Keyword::Copy);

		PARSE().one(&out->params);
		if (PARSE().tryEat(NamedOperator::Colon)) PARSE().one(&out->inits);
		PARSE().one(NamedOperator::Assign).withDef(&out->body, BlockOrderType::Ordered);

		PST_RETURN out;
	}

	void CopyConstructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		out << "\"" << getName().strView() << "\"";
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
