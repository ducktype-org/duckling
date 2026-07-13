#include "statement_tree.hpp"

#include "source_text.hpp"
#include "token_classes.hpp"

#include <utility>

namespace formatter {
	namespace {

		using lexer::Token;

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }

		/**
		 * After the terminating `;` at @p i, absorbs a line comment trailing it on the same
		 * source line (`x = 1; # note` keeps the note on the statement).
		 *
		 * @return The index of the first token after the statement.
		 */
		usize consumeTrailingComment(std::span<const Token> tokens, usize i, Statement& stmt) {
			const usize n = tokens.size();
			usize       j = i + 1;
			while (j < n && isSkippable(tokens[j])) j++;
			if (j < n && isLineComment(tokens[j]) && onSameSourceLine(tokens[i], tokens[j])) {
				stmt.end_comment = &tokens[j];
				return j + 1;
			}
			return i + 1;
		}

		/**
		 * Parses one statement starting at @p i into @p stmt.
		 *
		 * @return The index of the first token after the statement.
		 */
		usize parseStatement(std::span<const Token> tokens, usize i, Statement& stmt) {
			const usize n         = tokens.size();
			usize       run_start = i;
			bool        at_start  = true;

			const auto flush_run = [&](usize end) {
				for (usize k = run_start; k < end; k++)
					if (!isSkippable(tokens[k])) {
						stmt.items.emplace_back(RunItem{
							.tokens = tokens.subspan(run_start, end - run_start) });
						return;
					}
			};

			while (i < n) {
				const Token& t = tokens[i];
				if (isSkippable(t)) {
					i++;
					continue;
				}

				// A `case` keyword opens the next match arm, ending the current statement
				// even though no `;` precedes it.
				if (isCaseKeyword(t) && !at_start) {
					flush_run(i);
					return i;
				}
				at_start = false;

				// Statement terminator.
				if (isSpecial(t) && isStr(t, ";")) {
					flush_run(i);
					stmt.semicolon = true;
					return consumeTrailingComment(tokens, i, stmt);
				}

				// A line comment consumes the rest of its line, so it ends the statement.
				if (isLineComment(t)) {
					flush_run(i);
					stmt.end_comment = &t;
					return i + 1;
				}

				// A code block ends the statement (unless an `else` clause or `;` follows).
				if (isBlockCurly(t)) {
					flush_run(i);
					stmt.items.emplace_back(BlockItem{
						.curly = &t,
						.body  = parseStatementList(t.getRecursive()),
					});

					usize j = i + 1;
					while (j < n && isSkippable(tokens[j])) j++;

					if (j < n && isKeyword(tokens[j]) && isStr(tokens[j], "else")) {
						run_start = j;
						i         = j;
						continue;
					}
					if (j < n && isSpecial(tokens[j]) && isStr(tokens[j], ";")) {
						stmt.semicolon = true;
						return consumeTrailingComment(tokens, j, stmt);
					}
					return j;
				}

				i++;
			}
			flush_run(n);
			return n;
		}
	}

	std::vector<Statement> parseStatementList(std::span<const Token> tokens) {
		std::vector<Statement> statements;
		const Token*           prev_last = nullptr;
		const usize            n         = tokens.size();
		usize                  i         = 0;
		while (i < n) {
			if (isSkippable(tokens[i])) {
				i++;
				continue;
			}
			Statement stmt;
			if (prev_last != nullptr)
				stmt.blank_lines_before = emptyLinesBetween(*prev_last, tokens[i]);
			const usize next = parseStatement(tokens, i, stmt);
			for (usize j = next; j > i; j--)
				if (!isSkippable(tokens[j - 1])) {
					prev_last = &tokens[j - 1];
					break;
				}
			statements.push_back(std::move(stmt));
			i = next;
		}
		return statements;
	}
}
