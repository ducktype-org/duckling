#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	ParserRef<While> While::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<While>(position);

		if (!assertStmtChoice<While>(state, state.ctokens().is(Keyword::While))) return nullptr;

		parseAll(state, Keyword::While, &out->optional_name, &out->condition, &out->body);

		return out;
	}

	void While::dprint(std::ostream& out) const {
		out << R"({"While": {"name":)";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}

	void While::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitWhile(*this); }
}
