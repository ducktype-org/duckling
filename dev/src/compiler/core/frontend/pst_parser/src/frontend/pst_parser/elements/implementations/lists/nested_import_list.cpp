#include "../../hierarchy/lists/nested_import_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<NestedImportList> NestedImportList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ImportChain,
			NestedImportList,
			true,  // empty list not allowed
			true,  // trailing separator allowed
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::nestedImportList>(state);
	}
}
