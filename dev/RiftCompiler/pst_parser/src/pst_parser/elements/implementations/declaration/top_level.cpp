#include "forward.hpp"

namespace pst {
	ParserRef<TopLevel> TopLevel::parse(RiftParserState& state) {
		auto out = makeRef<TopLevel>(state.getPosition());
		while (state.notEmpty()) {
			ParserRef<Stmt> stmt;
			state.parse(out).one(&stmt);
			out->statements.emplace_back(std::move(stmt));
		}
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
