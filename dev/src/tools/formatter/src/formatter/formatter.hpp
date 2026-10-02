/**
 * @file formatter.hpp
 * @brief Token-based, configurable source formatter for Duckling.
 *
 * The formatter re-renders the token stream produced by the lexer with
 * normalized whitespace and indentation. Because it works on the full token
 * stream (comments included), the result is round-trip safe: re-tokenizing the
 * formatted output yields the same sequence of significant tokens. The single
 * exception is an over-long line comment, which is re-flowed onto several `#`
 * lines (the comment text is preserved, but one comment token becomes several).
 *
 * @note Comments only appear in the token stream when the source is tokenized
 *       with comments kept (`tokenize({ .keep_comments = true })`); otherwise the lexer discards
 *       them and formatting would drop them from the output.
 */
#pragma once

#include "config.hpp"

#include <lexer/token.hpp>

#include <string>

namespace formatter {

	/**
	 * @brief Formats a tokenized Duckling source file.
	 *
	 * Tokens are self-describing: verbatim text (e.g. format strings) and source layout
	 * (blank lines, trailing comments) are recovered through the token positions, which
	 * reference the tokenizer::TokenSource the tokens came from.
	 *
	 * @param tokens Tokenization result of the source (see tokenizer::TokenSource).
	 * @param config Formatting options.
	 * @return The formatted source, terminated by a single trailing newline; an empty string
	 *         for input that holds no significant token.
	 * @throws base::LogicError if brackets nest deeper than the formatter can recurse through.
	 */
	[[nodiscard]]
	std::string formatTokens(const lexer::TokenData& tokens, const FormatConfig& config);
}
