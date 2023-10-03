/**
 * @file token_stream.cpp
 */

#include "token_stream.hpp"

#include <base/exceptions.hpp>

namespace tpc {
	void tokenStreamInit() {
		// @TODO: change this to custom sentinel for each stream
		TokenStream::sentinel = lexer::Token::makeSentinel();
	}

	lexer::Token TokenStream::sentinel;

	TokenStream::TokenStream(TokenStream&& stream) noexcept:
		  tokens(stream.tokens),
		  where(stream.where),
		  to(stream.to) {}

	TokenStream TokenStream::getRecursive() const {
		if (peek().isGroup()) {
			const auto& rec = peek().getRecursive();
			return TokenStream(rec, 0, rec.size());
		} else {
			// @TODO
			throw base::LogicError("get recursive on no group");
		}
	}

	const Token& TokenStream::next() { return (where >= to ? sentinel : tokens[where++]); }

	const Token& TokenStream::peek(usize fwd) const {
		return (where + fwd >= to ? sentinel : tokens[where + fwd]);
	}

	void TokenStream::skip(usize n) { where += n; }

	bool TokenStream::isKeyword(usize fwd) const { return peek(fwd).isKeyword(); }

	Keyword TokenStream::asKeyword(usize fwd) const { return peek(fwd).asKeyword(); }

	bool TokenStream::isSpecial(usize fwd) const { return peek(fwd).isSpecial(); }

	Special TokenStream::asSpecial(usize fwd) const { return peek(fwd).asSpecial(); }

	bool TokenStream::isOperator(usize fwd) const { return peek(fwd).isOperator(); }

	usize TokenStream::size() const { return to - where; }

}  // namespace tpc
