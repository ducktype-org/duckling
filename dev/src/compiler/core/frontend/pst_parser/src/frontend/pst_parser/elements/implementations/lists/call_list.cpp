#include "../../hierarchy/lists/call_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<CallList> CallList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			CallArgument,
			CallList,
			false,  // empty list allowed
			true,   // trailing separator allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::callList>(state);
	}

}
