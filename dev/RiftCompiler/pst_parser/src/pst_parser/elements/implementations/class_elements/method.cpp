#include "preamble.hpp"

namespace pst {
	ParserRef<Method> Method::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Method>(position);

		out->parseSpecifiers(state);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		state.parse(out).all(Keyword::Fun, &out->name, &out->params);
		if (state.parse(out).tryEat(Operator::SingleArrow)) state.parse(out).one(&out->rets);

		state.parse(out).one(&out->body);

		return out;
	}

	void Method::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"params\":";
		nullAwareDprint(params, out);
		out << ",\"rets\":";
		nullAwareDprint(rets, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Method::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitMethod(*this); }
}
