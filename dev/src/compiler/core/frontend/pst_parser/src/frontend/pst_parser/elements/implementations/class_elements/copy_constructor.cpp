#include "../../hierarchy/class_elements/copy_constructor.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<CopyConstructor> CopyConstructor::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<CopyConstructor>(position);

		state.parse(out).eatOne();

		out->kind = Keyword::Copy;
		state.parse(out).all(NamedOperator::Period, Keyword::Copy);

		state.parse(out).one(&out->params);
		if (state.parse(out).tryEat(NamedOperator::Colon)) state.parse(out).one(&out->inits);

		state.parse(out).fallbackLen(state.ctokens().size());

		state.setConstextBlockOrdering(BlockOrderType::Ordered);
		state.parse(out).all(NamedOperator::Assign, &out->body);

		state.parse(out).exitFallback();

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
