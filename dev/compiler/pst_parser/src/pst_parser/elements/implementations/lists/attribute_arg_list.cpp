#include "../../hierarchy/lists/attribute_arg_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<AtrArgList> AtrArgList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			AtrArgList,
			false,
			lexer::Token::BracketType::Round,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::attributeArgList>(state);
	}
}
