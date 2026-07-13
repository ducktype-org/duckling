/**
 * @file token_classes.hpp
 * @brief Token classification predicates used by the formatter pipeline.
 */
#pragma once

#include <lexer/token.hpp>

#include <string_view>

namespace formatter {

	[[nodiscard]]
	std::string_view sv(const lexer::Token& t);

	[[nodiscard]]
	bool isOperator(const lexer::Token& t);
	[[nodiscard]]
	bool isSpecial(const lexer::Token& t);
	[[nodiscard]]
	bool isKeyword(const lexer::Token& t);
	[[nodiscard]]
	bool isComment(const lexer::Token& t);
	[[nodiscard]]
	bool isBracketGroup(const lexer::Token& t);

	/** A `# ...` comment that runs to the end of its line (as opposed to `#{ ... #}`). */
	[[nodiscard]]
	bool isLineComment(const lexer::Token& t);

	/** Tokens carrying no source text that must never be rendered. */
	[[nodiscard]]
	bool isSkippable(const lexer::Token& t);

	/** A token that closes an expression and so binds tightly to a following call/index group. */
	[[nodiscard]]
	bool isValueCloser(const lexer::Token& t);

	/** Prefix-unary candidates whose right operand must not be separated by a space. */
	[[nodiscard]]
	bool isSignOperator(const lexer::Token& t);

	/** A `++`/`--` that binds to the preceding value as a suffix: `t++`. */
	[[nodiscard]]
	bool isSuffixOperator(const lexer::Token& t);

	/** Member-access operators that bind tightly to the value on their left: `a.b`, `a.*`, `a.?`. */
	[[nodiscard]]
	bool isMemberAccessOperator(const lexer::Token& t);

	/** Built-in type keywords, which bind tightly to a following bracket group: `i32[5]`. */
	[[nodiscard]]
	bool isTypeKeyword(const lexer::Token& t);

	/** The `case` keyword, which starts a new match arm (and so a new statement). */
	[[nodiscard]]
	bool isCaseKeyword(const lexer::Token& t);

	/**
	 * A keyword that opens a statement (`return`, `var`, `while`, `fun`, ...). Its presence in
	 * a curly group marks the group as a code block rather than a collection literal, even when
	 * the block holds a single statement with no trailing `;` (`{return n}`).
	 */
	[[nodiscard]]
	bool isStatementKeyword(const lexer::Token& t);

	/**
	 * Whether `prev` leaves us at a position where an expression may start
	 * (so that a sign operator is unary rather than binary).
	 */
	[[nodiscard]]
	bool isPrefixContext(const lexer::Token* prev);

	/**
	 * Whether a curly group is a code block (rendered as one statement per line) as opposed to
	 * an inline literal such as `{1, 2, 3}` or an empty `{}`. A curly is a block when it contains
	 * a `;`, a nested curly block, a comment, a `case` arm, or a statement-opening keyword.
	 */
	[[nodiscard]]
	bool isBlockCurly(const lexer::Token& t);

	/**
	 * Whether a group may explode across lines: a `(...)`, `[...]` or `<...>` group that holds
	 * at least one top-level comma. (Curly code blocks break through their statement structure.)
	 */
	[[nodiscard]]
	bool isBreakableBracket(const lexer::Token& t);
}
