#include "elements_implementation.hpp"

namespace pst {
	ParserRef<RetList> RetList::parse(RiftParserState& state) {
		auto out = makeRef<RetList>(state.ctokens().peek().getPosition());

		constexpr auto isCurlyGroupStart = 
			[](const RiftParserState& lstate, usize fwd){
				return lstate.ctokens().isBracketGroup(Token::BracketType::Curly, fwd);
			};

		parseList<true>(state, out->rets, Operator::Comma, isCurlyGroupStart);

		return out;
	}

	void RetList::dprint(std::ostream& out) const {
		out << "{\"RetList\" : [";
		for (auto& x: rets) {
			nullAwareDprint(x, out);
			out << ",";
		}
		out << "]}";
	}
}
