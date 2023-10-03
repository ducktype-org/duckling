#include "elements_implementation.hpp"

namespace pst {
	ParserRef<ParamList> ParamList::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();

		if (!state.ctokens().is(Token::Type::RoundGroup)) {
			state.fail(-1, "parenthesis expected after here");
			return nullptr;
		}

		auto out = makeRef<ParamList>(position);
		state.goDown();

		parseList<false>(state, out->params, Operator::Comma, Token::Type::Sentinel);

		state.goUpAndSkip();
		return out;
	}

	void ParamList::dprint(std::ostream& out) const {
		out << "{\"ParamList\" : [";
		for (auto& x: params) {
			nullAwareDprint(x, out);
			out << ",";
		}
		out << "]}";
	}
}  // namespace pst
