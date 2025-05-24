#include "../../hierarchy/lists/parameter_list.hpp"

#include "../../hierarchy/not_statements/fun_param.hpp"
#include "impl_template.hpp"

namespace pst {
	MBox<ParamList> ParamList::parse(LangParserState& state) {
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
