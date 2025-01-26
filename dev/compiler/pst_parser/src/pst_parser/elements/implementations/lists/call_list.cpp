#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	MBox<CallList> CallList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolderLowerLevel,
			CallList,
			false,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::callList>(state);
	}
}
