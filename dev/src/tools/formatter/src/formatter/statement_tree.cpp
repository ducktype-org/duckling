// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "statement_tree.hpp"

#include "source_text.hpp"
#include "token_classes.hpp"

#include <utility>

namespace formatter {
	namespace {

		using lexer::Token;
		using Bracket = Token::BracketType;

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }

		/**
		 * After the last token of a statement at @p i, absorbs a comment trailing it on the
		 * same source line (`x = 1; # note` keeps the note on the statement, and so does
		 * `} # done`).
		 *
		 * @return The index of the first token after the statement.
		 */
		usize consumeTrailingComment(std::span<const Token> tokens, usize i, Statement& stmt) {
			const usize n = tokens.size();
			usize       j = i + 1;
			while (j < n && isSkippable(tokens[j])) j++;
			if (j < n && isComment(tokens[j]) && onSameSourceLine(tokens[i], tokens[j])) {
				stmt.end_comment = &tokens[j];
				return j + 1;
			}
			return i + 1;
		}

		/** Index of the first significant token at or after @p i, or `tokens.size()`. */
		usize nextSignificant(std::span<const Token> tokens, usize i) {
			while (i < tokens.size() && isSkippable(tokens[i])) i++;
			return i;
		}

		/**
		 * At the start of a statement, finds where a leading item that the author put on lines
		 * of its own ends: a block comment (`#{ ... #}` above a declaration) or an attribute
		 * (`@name` or `@name(...)` above a `fun`). Such an item becomes a statement of its own,
		 * so the line break after it survives.
		 *
		 * @return The index just past the item, or 0 when @p i opens no such item or the
		 *         next token sits on the same source line.
		 */
		usize ownLineItemEnd(std::span<const Token> tokens, usize i) {
			const usize n    = tokens.size();
			usize       last = i;
			if (isSpecial(tokens[i]) && isStr(tokens[i], "@")) {
				last = nextSignificant(tokens, i + 1);
				if (last == n) return 0;
				const usize args = nextSignificant(tokens, last + 1);
				if (args < n && isBracketGroup(tokens[args])
				    && tokens[args].getBracketType() == Bracket::Round
				    && onSameSourceLine(tokens[last], tokens[args]))
					last = args;
			} else if (!isComment(tokens[i]) || isLineComment(tokens[i])) {
				return 0;
			}
			const usize next = nextSignificant(tokens, last + 1);
			if (next == n || onSameSourceLine(tokens[last], tokens[next])) return 0;
			return last + 1;
		}

		/**
		 * Whether the curly group at @p i is an empty body that closes its line, as in
		 * `fun f() = {}` or `class C {}` followed by the next declaration. Such a group ends
		 * the statement like a code block; without content it cannot be told apart from an
		 * empty literal, so only the line break after it decides.
		 */
		bool isEmptyBodyEndingLine(std::span<const Token> tokens, usize i) {
			const Token& t = tokens[i];
			if (!isBracketGroup(t) || t.getBracketType() != Bracket::Curly) return false;
			for (const auto& child: t.getRecursive())
				if (!isSkippable(child)) return false;
			const usize next = nextSignificant(tokens, i + 1);
			if (next == tokens.size()) return false;
			const Token& after = tokens[next];
			return !onSameSourceLine(t, after) && !isOperator(after) && !isSpecial(after);
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
				// A `template(...)` header lives on its own line: it ends the statement it opens,
				// so the declaration it applies to starts the next one.
				if (at_start && isKeyword(t) && isStr(t, "template")) {
					usize j = i + 1;
					while (j < n && isSkippable(tokens[j])) j++;
					if (j < n && isBracketGroup(tokens[j])
					    && tokens[j].getBracketType() == Bracket::Round) {
						flush_run(j + 1);
						return j + 1;
					}
				}
				if (at_start) {
					if (const usize end = ownLineItemEnd(tokens, i); end != 0) {
						flush_run(end);
						return end;
					}
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
				if (isBlockCurly(t) || isEmptyBodyEndingLine(tokens, i)) {
					flush_run(i);
					stmt.items.emplace_back(BlockItem{
						.curly = &t,
						.body  = parseStatementList(t.getRecursive()),
					});

					usize j = i + 1;
					while (j < n && isSkippable(tokens[j])) j++;

					// So does an operator right after the brace, on the same line: the block
					// is an operand (`acc + match (v) {...} * 2;`), not a statement of its own.
					const bool continues
						= j < n
					   && ((isKeyword(tokens[j]) && isStr(tokens[j], "else"))
					       || (isOperator(tokens[j]) && onSameSourceLine(t, tokens[j])));
					if (continues) {
						run_start = j;
						i         = j;
						continue;
					}
					if (j < n && isSpecial(tokens[j]) && isStr(tokens[j], ";")) {
						stmt.semicolon = true;
						return consumeTrailingComment(tokens, j, stmt);
					}
					return consumeTrailingComment(tokens, i, stmt);
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
