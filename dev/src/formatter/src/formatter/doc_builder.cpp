#include "doc_builder.hpp"

#include "source_text.hpp"
#include "spacing.hpp"
#include "token_classes.hpp"

#include <base/types/ints.hpp>

#include <algorithm>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>

namespace formatter {
	namespace {

		using lexer::Token;
		using Bracket = Token::BracketType;

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }

		/** First token of @p tokens that carries source text, or nullptr. */
		const Token* firstSignificant(std::span<const Token> tokens) {
			for (const auto& t: tokens)
				if (!isSkippable(t)) return &t;
			return nullptr;
		}

		/** Last token of @p tokens that carries source text, or nullptr. */
		const Token* lastSignificant(std::span<const Token> tokens) {
			for (const auto& t: std::views::reverse(tokens))
				if (!isSkippable(t)) return &t;
			return nullptr;
		}

		/** One comma-separated group element, plus whether a comma followed it in source. */
		struct RawElement final {
			std::span<const Token> tokens;
			bool                   trailing_comma;
		};

		/**
		 * A group element refined for layout: its inline body (mid-element line comments
		 * included), plus the line comments rendered after its comma on the same line.
		 */
		struct GroupElement final {
			std::span<const Token>    body;
			bool                      trailing_comma = false;
			std::vector<const Token*> trailing_comments;
		};

		/**
		 * Splits a group's contents into top-level comma-separated elements, recording each
		 * trailing comma so both layouts reproduce the original token stream exactly. A
		 * residual element with no significant token (e.g. after a trailing comma) is dropped.
		 */
		std::vector<RawElement> splitElements(std::span<const Token> tokens) {
			std::vector<RawElement> elements;
			usize                   start = 0;
			for (usize i = 0; i < tokens.size(); i++) {
				if (isSpecial(tokens[i]) && isStr(tokens[i], ",")) {
					elements.push_back({
						.tokens         = tokens.subspan(start, i - start),
						.trailing_comma = true,
					});
					start = i + 1;
				}
			}
			if (start < tokens.size() && firstSignificant(tokens.subspan(start)) != nullptr)
				elements.push_back({
					.tokens         = tokens.subspan(start),
					.trailing_comma = false,
				});
			return elements;
		}

		/**
		 * @brief Builds Doc nodes from statements and token runs.
		 */
		class DocBuilder final {
		public:
			explicit DocBuilder(const FormatConfig& config): config(config) {}

			Doc buildFile(const std::vector<Statement>& statements) {
				std::vector<Doc> docs;
				appendStatementList(docs, statements);
				return doc::concat(std::move(docs));
			}

		private:
			const FormatConfig& config;

			void appendStatementList(
				std::vector<Doc>& docs, const std::vector<Statement>& statements
			) {
				bool first = true;
				for (const auto& stmt: statements) {
					if (!first)
						docs.push_back(doc::line(static_cast<u32>(
							std::min<usize>(stmt.blank_lines_before, config.max_empty_lines)
						)));
					first = false;
					docs.push_back(statementDoc(stmt));
				}
			}

			Doc statementDoc(const Statement& stmt) {
				std::vector<Doc> docs;
				const Token*     prev = nullptr;
				for (const auto& item: stmt.items) {
					if (const auto* run = std::get_if<RunItem>(&item)) {
						appendRun(docs, run->tokens, prev);
					} else {
						const auto& block = std::get<BlockItem>(item);
						// Separate the brace from a preceding token (`fun f() = {`), but not
						// when the block opens the statement (indentation positions it).
						if (prev != nullptr) docs.push_back(doc::space());
						docs.push_back(blockDoc(block));
						prev = block.curly;
					}
				}
				if (stmt.semicolon) docs.push_back(doc::text(";"));
				if (stmt.end_comment != nullptr) {
					if (stmt.semicolon || needSpace(config, prev, *stmt.end_comment))
						docs.push_back(doc::space());
					docs.push_back(doc::lineComment(std::string(sv(*stmt.end_comment))));
				}
				return doc::concat(std::move(docs));
			}

			/**
			 * Renders a `{...}` code block: statements one per line, one level deeper, the
			 * closing brace at the enclosing indentation. Inside a flat context (a fitting
			 * bracket group) the block flattens to `{stmt; stmt;}`; an empty body is `{}`.
			 */
			Doc blockDoc(const BlockItem& block) {
				if (block.body.empty()) return doc::text("{}");
				std::vector<Doc> inner;
				inner.push_back(doc::softLine());
				appendStatementList(inner, block.body);
				return doc::concat({
					doc::text("{"),
					doc::indent(std::move(inner)),
					doc::softLine(),
					doc::text("}"),
				});
			}

			/**
			 * Indices of method-chain dots: a `.` that follows a call or index group, as in
			 * `foo(a).bar`. A line may break before each of them.
			 */
			[[nodiscard]]
			std::vector<usize> chainBreakPoints(std::span<const Token> tokens) const {
				std::vector<usize> points;
				const Token*       prev = nullptr;
				for (usize i = 0; i < tokens.size(); i++) {
					const Token& t = tokens[i];
					if (isSkippable(t)) continue;
					if (prev != nullptr && isOperator(t) && isStr(t, ".") && isBracketGroup(*prev)) {
						const Bracket b = prev->getBracketType();
						if (b == Bracket::Round || b == Bracket::Square) points.push_back(i);
					}
					prev = &t;
				}
				return points;
			}

			/**
			 * Indices of binary operators rendered with a space before them, as in `a + b`.
			 * A line may break before each of them. Member dots are excluded (they bind
			 * tightly); so are unary operators (their left neighbor is not a value).
			 */
			[[nodiscard]]
			std::vector<usize> operatorBreakPoints(std::span<const Token> tokens) const {
				std::vector<usize> points;
				const Token*       prev = nullptr;
				for (usize i = 0; i < tokens.size(); i++) {
					const Token& t = tokens[i];
					if (isSkippable(t)) continue;
					if (prev != nullptr && isOperator(t) && !isStr(t, ".") && isValueCloser(*prev)
					    && needSpace(config, prev, t))
						points.push_back(i);
					prev = &t;
				}
				return points;
			}

			[[nodiscard]]
			static bool hasBreakableGroup(std::span<const Token> tokens) {
				return std::ranges::any_of(tokens, isBreakableBracket);
			}

			/**
			 * Appends a token run, choosing its wrapping strategy: break at method-chain dots
			 * when the run holds any; otherwise leave wrapping to a breakable bracket group when
			 * one is present (exploding it beats an operator break); otherwise break around
			 * spaced binary operators. Runs with no break points render inline.
			 *
			 * @param prev The token rendered just before the run, for spacing and unary operator
			 *             decisions across the boundary; updated to the run's last token.
			 */
			void appendRun(std::vector<Doc>& docs, std::span<const Token> run, const Token*& prev) {
				const auto chain_points = chainBreakPoints(run);
				if (!chain_points.empty()) {
					docs.push_back(fillDoc(run, chain_points, /*spaced=*/false, prev));
					return;
				}
				if (hasBreakableGroup(run)) {
					appendTokens(docs, run, prev, /*suppress_leading=*/false);
					return;
				}
				const auto operator_points = operatorBreakPoints(run);
				if (!operator_points.empty()) {
					docs.push_back(fillDoc(run, operator_points, /*spaced=*/true, prev));
					return;
				}
				appendTokens(docs, run, prev, /*suppress_leading=*/false);
			}

			/**
			 * Builds a Fill whose parts are @p run split before each break point. The first
			 * part keeps its leading space (it always continues the current line); later parts
			 * leave the separating space to the Fill, so a broken part starts its line clean.
			 */
			Doc fillDoc(
				std::span<const Token>    run,
				const std::vector<usize>& break_points,
				bool                      spaced,
				const Token*&             prev
			) {
				std::vector<Doc> parts;
				usize            start = 0;
				bool             first = true;

				const auto flush_part = [&](usize end) {
					if (end == start) return;
					const auto segment = run.subspan(start, end - start);
					start              = end;
					if (firstSignificant(segment) == nullptr) return;
					std::vector<Doc> part;
					appendTokens(part, segment, prev, /*suppress_leading=*/!first);
					parts.push_back(doc::concat(std::move(part)));
					first = false;
				};

				for (const usize point: break_points) flush_part(point);
				flush_part(run.size());
				return doc::fill(spaced, std::move(parts));
			}

			/**
			 * Appends tokens joined per the spacing oracle, with the unary-operator rule: a sign
			 * operator in prefix context glues to its operand. Bracket groups recurse.
			 *
			 * @param suppress_leading Whether to omit the space before the first token (used when
			 *                         the containing node starts a fresh line).
			 */
			void appendTokens(
				std::vector<Doc>&      docs,
				std::span<const Token> tokens,
				const Token*&          prev,
				bool                   suppress_leading
			) {
				bool suppress_space = suppress_leading;
				for (const auto& t: tokens) {
					if (isSkippable(t)) continue;
					const bool leading = suppress_space ? false : needSpace(config, prev, t);
					suppress_space     = false;
					if (leading) docs.push_back(doc::space());
					const bool unary = isSignOperator(t) && isPrefixContext(prev);
					docs.push_back(atomDoc(t));
					if (unary) suppress_space = true;
					prev = &t;
				}
			}

			Doc atomDoc(const Token& t) {
				if (isBracketGroup(t)) {
					if (isBlockCurly(t))
						return blockDoc({
							.curly = &t,
							.body  = parseStatementList(t.getRecursive()),
						});
					return groupDoc(t);
				}
				return doc::text(atomText(t));
			}

			/**
			 * Renders a bracket group. Flat when it fits; a breakable group (one holding a
			 * top-level comma) explodes one element per line when over-long or when it contains
			 * a line comment. Line comments attach to the element they trail in the source, so
			 * exploding never merges a comment into the following element.
			 */
			Doc groupDoc(const Token& t) {
				std::string open;
				appendUtf8(open, static_cast<char32_t>(t.getBracketType()));
				std::string close;
				appendUtf8(close, closingBracket(t.getBracketType()));

				const auto elements = refineElements(splitElements(t.getRecursive()));
				if (elements.empty()) return doc::concat({ doc::text(open), doc::text(close) });

				std::vector<Doc> inner;
				inner.push_back(doc::softLine());
				bool first = true;
				for (const auto& element: elements) {
					// An element fully absorbed by comment attachment renders nothing.
					if (firstSignificant(element.body) == nullptr && !element.trailing_comma
					    && element.trailing_comments.empty())
						continue;
					if (!first) inner.push_back(doc::line());
					first = false;
					inner.push_back(elementDoc(element));
				}
				return doc::group(
					isBreakableBracket(t),
					{
						doc::text(std::move(open)),
						doc::indent(std::move(inner)),
						doc::softLine(),
						doc::text(std::move(close)),
					}
				);
			}

			/**
			 * Attaches line comments to their elements: a comment on the same source line as the
			 * end of an element (before or after its comma) trails that element; any other
			 * comment stays in the body and gets its own line.
			 */
			static std::vector<GroupElement> refineElements(const std::vector<RawElement>& raw) {
				std::vector<GroupElement> elements;
				elements.reserve(raw.size());
				for (const auto& [tokens, trailing_comma]: raw) {
					GroupElement element{
						.body              = tokens,
						.trailing_comma    = trailing_comma,
						.trailing_comments = {},
					};

					// A comment opening this element that sits on the previous element's line
					// belongs after that element's comma: `foo(aaaa, # first`.
					if (!elements.empty()) {
						auto&        previous = elements.back();
						const Token* anchor   = lastSignificant(previous.body);
						const Token* head     = firstSignificant(element.body);
						if (anchor != nullptr && head != nullptr && isLineComment(*head)
						    && previous.trailing_comments.empty()
						    && onSameSourceLine(*anchor, *head)) {
							previous.trailing_comments.push_back(head);
							const auto offset = static_cast<usize>(head - element.body.data()) + 1;
							element.body      = element.body.subspan(offset);
						}
					}

					// A comment closing the element on its own line trails it directly:
					// `foo(aaaa # why` renders as `aaaa, # why` when a comma follows.
					while (true) {
						const Token* last        = nullptr;
						const Token* before_last = nullptr;
						for (const auto& tok: element.body) {
							if (isSkippable(tok)) continue;
							before_last = last;
							last        = &tok;
						}
						if (last == nullptr || !isLineComment(*last) || before_last == nullptr
						    || isLineComment(*before_last)
						    || !onSameSourceLine(*before_last, *last))
							break;
						element.trailing_comments.insert(element.trailing_comments.begin(), last);
						element.body = element.body.subspan(
							0, static_cast<usize>(last - element.body.data())
						);
					}

					elements.push_back(std::move(element));
				}
				return elements;
			}

			/**
			 * Renders one group element: its body (split at mid-body line comments, each ending
			 * its line), the trailing comma, then the comments trailing the comma.
			 */
			Doc elementDoc(const GroupElement& element) {
				std::vector<Doc> docs;
				const Token*     prev  = nullptr;
				usize            start = 0;
				const auto       body  = element.body;

				for (usize i = 0; i <= body.size(); i++) {
					const bool at_comment = i < body.size() && isLineComment(body[i]);
					if (i < body.size() && !at_comment) continue;
					const auto run = body.subspan(start, i - start);
					if (firstSignificant(run) != nullptr) appendRun(docs, run, prev);
					if (at_comment) {
						if (needSpace(config, prev, body[i])) docs.push_back(doc::space());
						docs.push_back(doc::lineComment(std::string(sv(body[i]))));
						docs.push_back(doc::line());
						prev = nullptr;
					}
					start = i + 1;
				}

				if (element.trailing_comma) docs.push_back(doc::text(","));
				for (const Token* comment: element.trailing_comments) {
					docs.push_back(doc::space());
					docs.push_back(doc::lineComment(std::string(sv(*comment))));
				}
				return doc::concat(std::move(docs));
			}
		};
	}

	Doc buildFileDoc(const std::vector<Statement>& statements, const FormatConfig& config) {
		return DocBuilder(config).buildFile(statements);
	}
}
