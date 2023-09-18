#include "elements_implementation.hpp"

namespace pst {
	ParserRef<RetList> RetList::parse(RiftParserState& state) {
		auto out = makeRef<RetList>(state.ctokens().peek().getPosition());

		parseList<true>(state, out->rets, Operator::Comma, Token::Type::CurlyGroup);

		return out;
	}

	void RetList::dprint(std::ostream& out) const {
		out << "{\"RetList\" : [";
		for (auto& x : rets) {
			nullAwareDprint(x, out);
			out << ",";
		}
		out << "]}";
	}
}
