#include "../../hierarchy/lists.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<CopyConstructor> CopyConstructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<CopyConstructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();

		out->kind = Keyword::Copy;
		state.parse(out).all(NamedOperator::Period, Keyword::Copy);

		state.parse(out).one(&out->params);
		if (state.parse(out).tryEat(NamedOperator::Colon)) state.parse(out).one(&out->inits);
		state.parse(out).all(NamedOperator::Assign, &out->body);

		return out;
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
