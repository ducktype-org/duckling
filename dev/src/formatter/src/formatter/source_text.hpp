/**
 * @file source_text.hpp
 * @brief Token text reproduction and source-layout queries for the formatter.
 *
 * Tokens are self-describing: their source positions reference the TokenSource they came from,
 * so verbatim text (`dia::SourcePosition::content()`) and line numbers are recovered from the
 * tokens alone — no separate copy of the source text is needed.
 */
#pragma once

#include <base/types/ints.hpp>

#include <lexer/token.hpp>

#include <string>

namespace formatter {

	/**
	 * @brief Renders a single non-group token as source text.
	 *
	 * String and char literals are restored with their delimiters (the lexeme omits them);
	 * format strings are reproduced verbatim from the source; any other token is its raw lexeme.
	 */
	[[nodiscard]]
	std::string atomText(const lexer::Token& t);

	/**
	 * @brief Appends the UTF-8 encoding of a Unicode code point to @p out.
	 * @note Encodes code points up to U+FFFF, which covers every bracket character the lexer
	 *       produces (the widest is the U+3008/U+3009 angle bracket pair).
	 */
	void appendUtf8(std::string& out, char32_t code);

	/** Returns the closing bracket code point that matches an opening bracket type. */
	[[nodiscard]]
	char32_t closingBracket(lexer::Token::BracketType open);

	/** Whether @p a and @p b sit on the same source line. */
	[[nodiscard]]
	bool onSameSourceLine(const lexer::Token& a, const lexer::Token& b);

	/** Number of empty source lines separating @p a from @p b. */
	[[nodiscard]]
	usize emptyLinesBetween(const lexer::Token& a, const lexer::Token& b);
}
