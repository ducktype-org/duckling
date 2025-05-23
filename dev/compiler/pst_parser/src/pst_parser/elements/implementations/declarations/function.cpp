#include "../../hierarchy/declarations/function.hpp"

#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	MBox<Fun> Fun::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Fun>(position);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		state.parse(out).all(Keyword::Fun, &out->name);
		state.parse(out).with<ParamList>(&out->params, ParamList::parse);

		if (state.parse(out).tryEat(NamedOperator::SingleArrow)) state.parse(out).one(&out->ret);

		state.parse(out).all(NamedOperator::Assign, &out->body);

		return out;
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Fun::acceptVisitor(PstVisitor& visitor) const { visitor.visitFun(*this); }
}
