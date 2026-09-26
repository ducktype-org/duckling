#include "source_text.hpp"

#include "token_classes.hpp"

#include <base/except/exceptions.hpp>

namespace formatter {
	namespace {

		using lexer::Token;
		using Bracket = Token::BracketType;

		usize startLine(const Token& t) { return t.getPosition().getStartLineColumn().first; }

		usize endLine(const Token& t) { return t.getPosition().getEndLineColumn().first; }
	}

	std::string atomText(const Token& t) {
		switch (t.getType()) {
		case Token::Type::String:
			return '"' + std::string(sv(t)) + '"';
		case Token::Type::Char:
			return '\'' + std::string(sv(t)) + '\'';
		case Token::Type::FormatString:
			return t.getPosition().content();
		default:
			return std::string(sv(t));
		}
	}

	void appendUtf8(std::string& out, char32_t code) {
		if (code < 0x80) {
			out.push_back(static_cast<char>(code));
		} else if (code < 0x8'00) {
			out.push_back(static_cast<char>(0xC0 | (code >> 6)));
			out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
		} else {
			out.push_back(static_cast<char>(0xE0 | (code >> 12)));
			out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
		}
	}

	char32_t closingBracket(Bracket open) {
		switch (open) {
		case Bracket::Round:
			return ')';
		case Bracket::Square:
			return ']';
		case Bracket::Curly:
			return '}';
		case Bracket::Angle:
			return 0x30'09;
		case Bracket::None:
			break;
		}
		CORE_PANIC("unknown bracket type in the formatter");
	}

	u32 visualWidth(std::string_view text) {
		const auto last_line = text.rfind('\n');
		if (last_line != std::string_view::npos) text.remove_prefix(last_line + 1);
		u32 width = 0;
		for (const char c: text)
			// Continuation bytes (0b10xxxxxx) belong to the code point already counted.
			if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) width++;
		return width;
	}

	bool onSameSourceLine(const Token& a, const Token& b) { return endLine(a) == startLine(b); }

	usize emptyLinesBetween(const Token& a, const Token& b) {
		const usize end   = endLine(a);
		const usize start = startLine(b);
		if (start <= end) return 0;
		return start - end - 1;
	}
}
