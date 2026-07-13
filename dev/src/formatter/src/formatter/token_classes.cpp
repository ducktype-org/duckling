#include "token_classes.hpp"

#include <lang_definitions/key_spec_op.hpp>

#include <algorithm>
#include <array>

namespace formatter {
	namespace {

		using lexer::Token;
		using Type    = Token::Type;
		using Bracket = Token::BracketType;

		bool isType(const Token& t, Type type) { return t.getType() == type; }

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }
	}

	std::string_view sv(const Token& t) { return t.getStrValue(); }

	bool isOperator(const Token& t) { return isType(t, Type::Operator); }

	bool isSpecial(const Token& t) { return isType(t, Type::Special); }

	bool isKeyword(const Token& t) { return isType(t, Type::Keyword); }

	bool isComment(const Token& t) { return isType(t, Type::Comment); }

	bool isBracketGroup(const Token& t) { return isType(t, Type::BracketGroup); }

	bool isLineComment(const Token& t) {
		return isComment(t) && sv(t).starts_with("#") && !sv(t).starts_with("#{");
	}

	bool isSkippable(const Token& t) { return isType(t, Type::Empty) || isType(t, Type::Sentinel); }

	bool isValueCloser(const Token& t) {
		switch (t.getType()) {
		case Type::Identifier:
		case Type::NumLiteral:
		case Type::NumLiteralGroup:
		case Type::String:
		case Type::Char:
		case Type::FormatString:
		case Type::BracketGroup:
			return true;
		default:
			return false;
		}
	}

	bool isSignOperator(const Token& t) {
		if (isKeyword(t) && isStr(t, "not")) return true;
		if (!isOperator(t)) return false;
		return isStr(t, "-") || isStr(t, "+") || isStr(t, "!") || isStr(t, "~") || isStr(t, "&")
		    || isStr(t, "?") || isStr(t, "++") || isStr(t, "--");
	}

	bool isSuffixOperator(const Token& t) {
		return isOperator(t) && (isStr(t, "++") || isStr(t, "--"));
	}

	bool isMemberAccessOperator(const Token& t) {
		return isOperator(t) && (isStr(t, ".") || isStr(t, ".*") || isStr(t, ".?"));
	}

	bool isTypeKeyword(const Token& t) {
		if (!isKeyword(t)) return false;
		constexpr auto TYPE_KEYWORDS = std::to_array<std::string_view>({
			"i8",   "i16",  "i32",    "i64",  "i128", "u8",  "u16",  "u32",
			"u64",  "u128", "f16",    "f32",  "f64",  "f80", "f128", "char",
			"bool", "str",  "String", "type", "List", "Set", "Dict", "Array",
		});
		return std::ranges::find(TYPE_KEYWORDS, sv(t)) != TYPE_KEYWORDS.end();
	}

	bool isCaseKeyword(const Token& t) { return isKeyword(t) && isStr(t, "case"); }

	bool isStatementKeyword(const Token& t) {
		return isKeyword(t)
		    && lang_def::keywordFlags(t.asKeyword())
		           .contains(lang_def::KeywordFlagsOptions::IsStmtStart);
	}

	bool isPrefixContext(const Token* prev) {
		if (prev == nullptr) return true;
		if (isOperator(*prev)) return true;
		if (isKeyword(*prev)) return true;
		if (isSpecial(*prev) && isStr(*prev, ",")) return true;
		return false;
	}

	bool isBlockCurly(const Token& t) {
		if (!isBracketGroup(t) || t.getBracketType() != Bracket::Curly) return false;
		for (const auto& child: t.getRecursive()) {
			if (isSkippable(child)) continue;
			if (isSpecial(child) && isStr(child, ";")) return true;
			if (isBracketGroup(child) && child.getBracketType() == Bracket::Curly) return true;
			if (isComment(child)) return true;
			// Match arms need no `;`, yet a match body is still a code block.
			if (isCaseKeyword(child)) return true;
			// A statement keyword (`return`, `while`, ...) marks a single-statement block
			// with no trailing `;`, e.g. `if (c) {return n}`.
			if (isStatementKeyword(child)) return true;
		}
		return false;
	}

	bool isBreakableBracket(const Token& t) {
		if (!isBracketGroup(t) || t.getBracketType() == Bracket::Curly) return false;
		for (const auto& child: t.getRecursive())
			if (isSpecial(child) && isStr(child, ",")) return true;
		return false;
	}
}
