#include "../../hierarchy/lists/parameter_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<ParamList> ParamList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			FunParam,
			ParamList,
			false,
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::parameterList>(state);
	}
}
