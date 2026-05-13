#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace compiler::repl::replxx_helpers {
	/// Extract a word (identifier-like) ending at position \p pos in \p input.
	std::string extractWordEndingAt(const std::string& input, size_t pos);

	/// Extract all identifier-like tokens from \p text.
	std::vector<std::string> tokenizeIdentifiers(const std::string& text);

	/**
	 * Compute indentation depth from unmatched braces up to \p cursor_pos.
	 * Braces inside strings or line comments are ignored.
	 */
	int computeBraceIndentDepth(const std::string& input, size_t cursor_pos);

	/**
	 * Constructs a simplified string where every UTF-8 code point from the input
	 * is represented by exactly 1 byte.
	 * Single-byte characters are kept as-is, while multi-byte characters
	 * are replaced by a specified constant byte.
	 *
	 * \param input - a UTF-8 encoded string.
	 * \param constant_byte - the character to substitute for multi-byte code points.
	 * By default it is '\1' - doesn't fall into any category and will be default color.
	 * \return A string whose .length() perfectly matches the number of code points.
	 */
	std::string mapUtf8CodePoints(const std::string& input, char constant_byte = '\1');
}
