#include "elements_common.hpp"

#include <token_parser_core/token_stream.hpp>

namespace pst::internal {
	bool Conditions::isComma(const TokenStream& state, i64 fwd) {
		return state[fwd].is(lang_def::Special::Comma);
	}

	bool Conditions::isSemicolon(const TokenStream& state, i64 fwd) {
		return state[fwd].is(lang_def::Special::Semicolon);
	}

	bool Conditions::isSentinel(const TokenStream& state, i64 fwd) {
		return state[fwd].is(lexer::Token::Type::Sentinel);
	}

	bool Conditions::isCurlyGroup(const TokenStream& state, i64 fwd) {
		return state[fwd].isBracketGroup(lexer::Token::BracketType::Curly);
	}

	bool Conditions::isAssignOrSemicolon(const TokenStream& st, i64 fwd) {
		return st[fwd].is(lang_def::NamedOperator::Assign)
		    || st[fwd].is(lang_def::Special::Semicolon);
	}

	bool Conditions::isAssignOrCommaOrEnd(const TokenStream& st, i64 fwd) {
		return st[fwd].is(lexer::Token::Type::Sentinel)
		    || st[fwd].is(lang_def::NamedOperator::Assign) || st[fwd].is(lang_def::Special::Comma);
	}

	bool Conditions::isBlockGroup(const TokenStream& st, i64 fwd) {
		return st[fwd].isBracketGroup(lexer::Token::Curly)
		    && not st[fwd - 1].is(lang_def::NamedOperator::Colon);
	}

	bool Conditions::isImplementsOrBlockGroup(const TokenStream& st, i64 fwd) {
		return st[fwd].is(lang_def::Keyword::Implements)
		    || (st[fwd].isBracketGroup(lexer::Token::Curly)
		        && not st[fwd - 1].is(lang_def::NamedOperator::Colon));
	}

	bool Conditions::isMatchBodyBlock(const TokenStream& st, i64 fwd) {
		return fwd >= 2 && st[fwd].isBracketGroup(lexer::Token::Curly)
		    && st[fwd - 1].isBracketGroup(lexer::Token::Round)
		    && st[fwd - 2].is(lang_def::Keyword::Match);
	}

	bool Conditions::isKeyword(const TokenStream& st, i64 fwd, lang_def::Keyword key) {
		return st[fwd].is(key);
	}
}
