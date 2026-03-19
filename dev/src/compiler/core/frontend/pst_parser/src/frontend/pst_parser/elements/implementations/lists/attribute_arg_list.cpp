#include "../../hierarchy/lists/attribute_arg_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<AtrArgList> AtrArgList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			AtrArgList,
			false,  // empty list allowed
			true,   // trailing separator allowed
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::attributeArgList>(state);
	}
}
