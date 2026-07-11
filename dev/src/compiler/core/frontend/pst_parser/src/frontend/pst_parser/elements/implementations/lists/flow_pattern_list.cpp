#include "../../hierarchy/lists/flow_pattern_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(FlowPatternList, elements);

	MBox<FlowPatternList> FlowPatternList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			FlowPattern,
			FlowPatternList,
			true,   // empty list not allowed
			false,  // trailing separator not allowed
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::flowPatternList>(state);
	}
}
