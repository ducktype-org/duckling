#include "spacing.hpp"

#include "token_classes.hpp"

namespace formatter {
	namespace {

		using lexer::Token;
		using Bracket = Token::BracketType;

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }
	}

	bool needSpace(const FormatConfig& config, const Token* prev, const Token& cur) {
		if (prev == nullptr) return false;

		// Two adjacent operator tokens must stay separated: the lexer greedily merges
		// consecutive operator characters, so e.g. `*`, `.`, `->` rendered as `*.->`
		// would re-tokenize into a single, different operator. This guard preserves the
		// round-trip property and overrides every no-space rule below.
		if (isOperator(*prev) && isOperator(cur)) return true;

		// No space before statement/list punctuation.
		if (isSpecial(cur) && (isStr(cur, ";") || isStr(cur, ","))) return false;

		// Member access binds tightly to the value on its left: `a.b`, `a.*`, `a.?b`.
		if (isMemberAccessOperator(cur)) return false;
		if (isOperator(*prev) && (isStr(*prev, ".") || isStr(*prev, ".?"))) return false;

		// Attribute sigil binds to its name: `@Attr`.
		if (isSpecial(*prev) && isStr(*prev, "@")) return false;

		// Suffix increment/decrement binds to the preceding value: `t++`.
		if (isSuffixOperator(cur) && isValueCloser(*prev)) return false;

		// Type/label colon: no space before, space after (handled by default).
		if (isOperator(cur) && isStr(cur, ":")) return false;

		// Call and index groups bind to the preceding value: `foo(x)`, `arr[i]`.
		if (isBracketGroup(cur)) {
			const Bracket b = cur.getBracketType();
			if (b == Bracket::Round || b == Bracket::Square) {
				if (isTypeKeyword(*prev)) return false;  // `i32[5]`, `List[i32]`
				if (isKeyword(*prev)) return true;       // `if (...)`, `return [...]`
				if (isValueCloser(*prev)) return false;  // `foo(...)`, `a[...]`
			}
			return true;
		}

		if (!config.space_around_operators && (isOperator(*prev) || isOperator(cur))) return false;

		return true;
	}
}
