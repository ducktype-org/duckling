#include "replxx_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace compiler::repl::replxx_helpers {
	std::string extractWordEndingAt(const std::string& input, size_t pos) {
		if (pos == 0 || pos > input.length()) return "";

		size_t start = pos;
		while (start > 0
		       && (std::isalnum(static_cast<unsigned char>(input[start - 1]))
		           || input[start - 1] == '_')) {
			--start;
		}

		return input.substr(start, pos - start);
	}

	std::vector<std::string> tokenizeIdentifiers(const std::string& text) {
		std::vector<std::string> tokens;
		std::string              current;
		for (char ch: text) {
			if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
				current += ch;
			} else if (!current.empty()) {
				tokens.push_back(std::move(current));
				current.clear();
			}
		}
		if (!current.empty()) tokens.push_back(std::move(current));
		return tokens;
	}

	int computeBraceIndentDepth(const std::string& input, size_t cursor_pos) {
		int  depth           = 0;
		bool in_single_quote = false;
		bool in_double_quote = false;
		bool escape_next     = false;

		bool in_single_line_comment  = false;
		int  multiline_comment_depth = 0;

		for (size_t i = 0; i < cursor_pos && i < input.size(); ++i) {
			const char ch = input[i];

			if (escape_next) {
				escape_next = false;
				continue;
			}

			if (in_single_line_comment) {
				if (ch == '\n') in_single_line_comment = false;
				continue;
			}

			if (in_single_quote) {
				if (ch == '\\')
					escape_next = true;
				else if (ch == '\'')
					in_single_quote = false;
				continue;
			}

			if (in_double_quote) {
				if (ch == '\\')
					escape_next = true;
				else if (ch == '"')
					in_double_quote = false;
				continue;
			}

			bool has_next = (i + 1 < cursor_pos && i + 1 < input.size());
			char next_ch  = has_next ? input[i + 1] : '\0';

			if (ch == '#' && next_ch == '{') {
				multiline_comment_depth++;
				++i;  // Skip the '{'
				continue;
			}

			if (ch == '#' && next_ch == '}') {
				if (multiline_comment_depth > 0) {
					multiline_comment_depth--;
					++i;  // Skip the '}'
					continue;
				}
			}

			if (ch == '#') {
				in_single_line_comment = true;
				continue;
			}

			if (multiline_comment_depth > 0) continue;

			if (ch == '\'') {
				in_single_quote = true;
				continue;
			}

			if (ch == '"') {
				in_double_quote = true;
				continue;
			}

			if (ch == '{')
				++depth;
			else if (ch == '}')
				depth = std::max(0, depth - 1);
		}

		return depth;
	}

	std::string mapUtf8CodePoints(const std::string& input, char constant_byte) {
		std::string result;

		result.reserve(input.length());

		for (size_t i = 0; i < input.length(); ++i) {
			auto c = static_cast<unsigned char>(input[i]);

			if ((c & 0x80) == 0x00) {
				// 1-byte code point (ASCII: 0xxxxxxx)
				result.push_back(input[i]);
			} else if ((c & 0xC0) == 0xC0) {
				// Leading byte of a multi-byte code point (11xxxxxx)
				// We substitute it with our chosen constant byte.
				result.push_back(constant_byte);
			} else {
				// Continuation byte (10xxxxxx)
				// We simply ignore these, so the multi-byte code point
				// only contributes exactly 1 byte to the `result` string.
			}
		}

		return result;
	}
}
