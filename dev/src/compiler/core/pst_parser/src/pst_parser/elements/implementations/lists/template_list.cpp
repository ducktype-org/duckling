#include "../../hierarchy/lists/template_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<TemplateList> TemplateList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolderLowerLevel,
			TemplateList,
			false,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::templateList>(state);
	}
}
