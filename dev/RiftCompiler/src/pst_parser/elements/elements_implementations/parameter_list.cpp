#include "elements_implementation.hpp"

namespace pst {
	ParserRef<ParamList> ParamList::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();

		if (!state.ctokens().isBracketGroup(Token::BracketType::Round)) {
			state.fail(-1, "parenthesis expected after here");
			return nullptr;
		}

		auto out = makeRef<ParamList>(position);
		state.goDown();

		constexpr auto isSentinel =
			[](const RiftParserState& lstate, usize fwd) {
				return lstate.ctokens().is(Token::Type::Sentinel, fwd);
			};

		parseList<false>(state, out->params, Operator::Comma, isSentinel);

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
}
