#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	MBox<TemplateList> TemplateList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ExprElement,
			TemplateList,
			false,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::templateList,
			UniversalExpr>(state);
	}
}