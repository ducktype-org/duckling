#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	MBox<AtrArgList> AtrArgList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ExprElement,
			AtrArgList,
			false,
			lexer::Token::BracketType::Round,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::attributeArgList,
			UniversalExpr>(state);
	}
}
