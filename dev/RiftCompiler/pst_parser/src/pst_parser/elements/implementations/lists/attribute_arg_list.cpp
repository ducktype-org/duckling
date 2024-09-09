#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	ParserRef<AtrArgList> AtrArgList::parse(RiftParserState& state) {
		return ListParsingTemplate::parseList<
			Expr,
			AtrArgList,
			false,
			lexer::Token::BracketType::Round,
			detail::Conditions::isComma,
			detail::Conditions::isSentinel,
			detail::NameGetters::attributeArgList>(state);
	}
}
