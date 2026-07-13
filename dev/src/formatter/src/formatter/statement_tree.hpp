/**
 * @file statement_tree.hpp
 * @brief Splits a token stream into statements (the formatter's structure pass).
 */
#pragma once

#include <base/types/ints.hpp>

#include <lexer/token.hpp>

#include <span>
#include <variant>
#include <vector>

namespace formatter {

	struct Statement;

	/** A maximal run of inline tokens between statement-level code blocks. */
	struct RunItem final {
		std::span<const lexer::Token> tokens;
	};

	/** A `{...}` code-block token at statement level, with its body pre-parsed. */
	struct BlockItem final {
		const lexer::Token*    curly;
		std::vector<Statement> body;
	};

	using StatementItem = std::variant<RunItem, BlockItem>;

	/**
	 * @brief One statement of a statement list.
	 *
	 * Runs and blocks interleave so that an `if/else if/else` chain stays a single statement:
	 * `[Run(if (a)), Block, Run(else if (c)), Block, Run(else), Block]`.
	 */
	struct Statement final {
		/** Empty source lines between the previous statement and this one (uncapped). */
		usize blank_lines_before = 0;

		std::vector<StatementItem> items;

		/** Whether a `;` terminates the statement. */
		bool semicolon = false;

		/** A line comment ending the statement, or trailing it on the `;` source line. */
		const lexer::Token* end_comment = nullptr;
	};

	/**
	 * @brief Splits @p tokens into statements.
	 *
	 * A statement ends at a `;`, at a line comment (which consumes the rest of its line), at a
	 * `case` keyword opening the next match arm, or after a `{...}` code block — unless the
	 * block is followed by `else` (the chain continues) or by `;` (absorbed as the terminator).
	 * A comment trailing the `;` on the same source line is attached as `end_comment`.
	 */
	[[nodiscard]]
	std::vector<Statement> parseStatementList(std::span<const lexer::Token> tokens);
}
