#include "../../hierarchy/lists/parameter_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ParamList, elements);

	MBox<ParamList> ParamList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			Param,
			ParamList,
			false,  // empty list allowed
			true,   // trailing separator not allowed
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::parameterList>(state);
	}
}
