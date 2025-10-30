#include "../../hierarchy/lists/flow_pattern_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<FlowPatternList> FlowPatternList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			FlowPattern,
			FlowPatternList,
			true,
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::flowPatternList>(state);
	}
}
