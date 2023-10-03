#include "elements_implementation.hpp"

namespace pst {
	ParserRef<TopLevel> TopLevel::parse(RiftParserState& state) {
		auto out = makeRef<TopLevel>(state.ctokens().peek().getPosition());
		while (state.notEmpty()) out->statements.emplace_back(Stmt::parse(state));
		return out;
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
}  // namespace pst
