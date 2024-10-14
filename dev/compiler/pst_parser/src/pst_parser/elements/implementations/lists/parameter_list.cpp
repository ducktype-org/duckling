#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	ParserRef<ParamList> ParamList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			FunParam,
			ParamList,
			false,
			lexer::Token::BracketType::Round,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::parameterList>(state);
	}
}
