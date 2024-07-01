#include "elements_implementation.hpp"

namespace pst {
	ParserRef<TopLevel> TopLevel::parse(RiftParserState& state) {
		auto out = makeRef<TopLevel>(state.getPosition());
		while (state.notEmpty()) out->statements.emplace_back(Stmt::parse(state));
		out->setLastToken(state.getPosition(-1));
		return out;
	}

	void TopLevel::acceptVisitor(PstStmtVisitor&) const {
		RIFT_PANIC("Visitng TopLevel statement");
	}

	void TopLevel::dprint(std::ostream& out) const {
		// @TODO: PST?
		out << "{\"PST\" : [";
		for (auto& e: statements) {
			nullAwareDprint(e, out);
			out << ", ";
		}
		out << "]}";
	}
}
