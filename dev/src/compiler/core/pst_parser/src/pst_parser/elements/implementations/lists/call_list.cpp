#include "../../hierarchy/lists/call_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<CallList> CallList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolderLowerLevel,
			CallList,
			false,
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::callList>(state);
	}
}
