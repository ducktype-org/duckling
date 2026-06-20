#include "formatter.hpp"

#include <base/types/ints.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <lexer/token.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace formatter {
	namespace {

		using lexer::Token;
		using lexer::Tokens;

		using Type    = Token::Type;
		using Bracket = Token::BracketType;

		std::string_view sv(const Token& t) { return t.getStrValue(); }

		bool isType(const Token& t, Type type) { return t.getType() == type; }

		bool isStr(const Token& t, std::string_view s) { return sv(t) == s; }

		bool isOperator(const Token& t) { return isType(t, Type::Operator); }

		bool isSpecial(const Token& t) { return isType(t, Type::Special); }

		bool isKeyword(const Token& t) { return isType(t, Type::Keyword); }

		bool isComment(const Token& t) { return isType(t, Type::Comment); }

		bool isBracketGroup(const Token& t) { return isType(t, Type::BracketGroup); }

		/** A `# ...` comment that runs to the end of its line (as opposed to `#{ ... #}`). */
		bool isLineComment(const Token& t) {
			return isComment(t) && sv(t).starts_with("#") && !sv(t).starts_with("#{");
		}

		/** Tokens carrying no source text that must never be rendered. */
		bool isSkippable(const Token& t) {
			return isType(t, Type::Empty) || isType(t, Type::Sentinel);
		}

		/** A token that closes an expression and so binds tightly to a following call/index group. */
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

		/** Prefix-unary candidates whose right operand must not be separated by a space. */
		bool isSignOperator(const Token& t) {
			if (isKeyword(t) && isStr(t, "not")) return true;
			if (!isOperator(t)) return false;
			return isStr(t, "-") || isStr(t, "+") || isStr(t, "!") || isStr(t, "~") || isStr(t, "&")
			    || isStr(t, "?") || isStr(t, "++") || isStr(t, "--");
		}

		/** A `++`/`--` that binds to the preceding value as a suffix: `t++`. */
		bool isSuffixOperator(const Token& t) {
			return isOperator(t) && (isStr(t, "++") || isStr(t, "--"));
		}

		/** Member-access operators that bind tightly to the value on their left: `a.b`, `a.*`,
		 * `a.?`. */
		bool isMemberAccessOperator(const Token& t) {
			return isOperator(t) && (isStr(t, ".") || isStr(t, ".*") || isStr(t, ".?"));
		}

		/** Built-in type keywords, which bind tightly to a following bracket group: `i32[5]`. */
		bool isTypeKeyword(const Token& t) {
			if (!isKeyword(t)) return false;
			constexpr auto TYPE_KEYWORDS = std::to_array<std::string_view>({
				"i8",   "i16",  "i32",    "i64",  "i128", "u8",  "u16",  "u32",
				"u64",  "u128", "f16",    "f32",  "f64",  "f80", "f128", "char",
				"bool", "str",  "String", "type", "List", "Set", "Dict", "Array",
			});
			return std::ranges::find(TYPE_KEYWORDS, sv(t)) != TYPE_KEYWORDS.end();
		}

		/** The `case` keyword, which starts a new match arm (and so a new statement). */
		bool isCaseKeyword(const Token& t) { return isKeyword(t) && isStr(t, "case"); }

		/**
		 * A keyword that opens a statement (`return`, `var`, `while`, `fun`, ...). Its presence in
		 * a curly group marks the group as a code block rather than a collection literal, even when
		 * the block holds a single statement with no trailing `;` (`{return n}`).
		 */
		bool isStatementKeyword(const Token& t) {
			return isKeyword(t)
			    && lang_def::keywordFlags(t.asKeyword())
			           .contains(lang_def::KeywordFlagsOptions::IsStmtStart);
		}

		/**
		 * Whether `prev` leaves us at a position where an expression may start
		 * (so that a sign operator is unary rather than binary).
		 */
		bool isPrefixContext(const Token* prev) {
			if (prev == nullptr) return true;
			if (isOperator(*prev)) return true;
			if (isKeyword(*prev)) return true;
			if (isSpecial(*prev) && isStr(*prev, ",")) return true;
			return false;
		}

		/**
		 * @brief Appends the UTF-8 encoding of a Unicode code point to @p out.
		 * @note Encodes code points up to U+FFFF, which covers every bracket character the lexer
		 *       produces (the widest is the U+3008/U+3009 angle bracket pair).
		 */
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

		/** Returns the closing bracket code point that matches an opening bracket type. */
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
			default:
				return static_cast<char32_t>(open);
			}
		}

		/**
		 * @brief Renders a token stream to formatted source text.
		 */
		class Emitter final {
		public:
			Emitter(const FormatConfig& config, std::string_view source):
				  config(config),
				  source(source) {}

			std::string run(const Tokens& tokens) {
				emitBlockBody(tokens);
				return std::move(out);
			}

		private:
			const FormatConfig& config;
			std::string_view    source;
			std::string         out;
			u32                 indent = 0;

			void writeIndent() {
				if (config.indent_style == IndentStyle::Tab)
					out.append(indent, '\t');
				else
					out.append(static_cast<usize>(indent) * config.indent_width, ' ');
			}

			/**
			 * Whether a curly group is a code block (rendered multi-line) as opposed to an
			 * inline literal such as `{1, 2, 3}` or an empty `{}`.
			 */
			[[nodiscard]]
			bool isBlockCurly(const Token& t) const {
				if (!isBracketGroup(t) || t.getBracketType() != Bracket::Curly) return false;
				for (const auto& child: t.getRecursive()) {
					if (isSkippable(child)) continue;
					if (isSpecial(child) && isStr(child, ";")) return true;
					if (isBracketGroup(child) && child.getBracketType() == Bracket::Curly)
						return true;
					if (isComment(child)) return true;
					// Match arms need no `;`, yet a match body is still a code block.
					if (isCaseKeyword(child)) return true;
					// A statement keyword (`return`, `while`, ...) marks a single-statement block
					// with no trailing `;`, e.g. `if (c) {return n}`.
					if (isStatementKeyword(child)) return true;
				}
				return false;
			}

			/** Decides whether a single space separates `prev` from `cur` in inline context. */
			bool needSpace(const Token* prev, const Token& cur) const {
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

				if (!config.space_around_operators && (isOperator(*prev) || isOperator(cur)))
					return false;

				return true;
			}

			/**
			 * Appends a token to @p buf exactly as it appears in the original source, falling back
			 * to its lexeme when no source span is available.
			 */
			void appendVerbatim(std::string& buf, const Token& t) const {
				const auto position = t.getPosition();
				const auto start    = position.getStart();
				const auto end      = position.getEnd();
				if (!source.empty() && start <= end && end < source.size())
					buf += source.substr(start, end - start + 1);
				else
					buf += sv(t);
			}

			/** Whether @p a and @p b sit on the same source line (no newline between them). */
			[[nodiscard]]
			bool onSameSourceLine(const Token& a, const Token& b) const {
				const auto end   = a.getPosition().getEnd();
				const auto start = b.getPosition().getStart();
				if (source.empty() || start <= end || start > source.size()) return false;
				const auto newline = source.find('\n', end);
				return newline == std::string_view::npos || newline >= start;
			}

			/** Number of empty source lines separating @p a from @p b. */
			[[nodiscard]]
			usize emptyLinesBetween(const Token& a, const Token& b) const {
				const auto end   = a.getPosition().getEnd();
				const auto start = b.getPosition().getStart();
				if (source.empty() || start <= end || start > source.size()) return 0;
				usize newlines = 0;
				for (usize i = end + 1; i < start; i++)
					if (source[i] == '\n') newlines++;
				return newlines == 0 ? 0 : newlines - 1;
			}

			/** Current column: characters emitted since the last newline. */
			[[nodiscard]]
			usize currentColumn() const {
				const auto newline = out.rfind('\n');
				return newline == std::string::npos ? out.size() : out.size() - newline - 1;
			}

			/**
			 * Whether a group may be split across lines: a `(...)`, `[...]` or `<...>` group that
			 * holds at least one top-level comma. (Curly code blocks wrap via emitBlockCurly.)
			 */
			[[nodiscard]]
			bool isBreakable(const Token& t) const {
				if (!isBracketGroup(t) || t.getBracketType() == Bracket::Curly) return false;
				for (const auto& child: t.getRecursive())
					if (isSpecial(child) && isStr(child, ",")) return true;
				return false;
			}

			/** One comma-separated group element, plus whether a comma followed it in source. */
			struct Element final {
				std::span<const Token> tokens;
				bool                   trailing_comma;
			};

			/**
			 * Splits a group's contents into top-level comma-separated elements, recording each
			 * trailing comma so the exploded form reproduces the original token stream exactly.
			 */
			[[nodiscard]]
			std::vector<Element> splitElements(const Tokens& tokens) const {
				std::vector<Element> elements;
				usize                start = 0;
				for (usize i = 0; i < tokens.size(); i++) {
					if (isSpecial(tokens[i]) && isStr(tokens[i], ",")) {
						elements.push_back({
							.tokens         = { tokens.data() + start, tokens.data() + i },
							.trailing_comma = true,
						});
						start = i + 1;
					}
				}
				if (start < tokens.size())
					elements.push_back({
						.tokens         = { tokens.data() + start, tokens.data() + tokens.size() },
						.trailing_comma = false,
					});
				return elements;
			}

			/* --- inline rendering: measures groups and emits the ones that fit on one line --- */

			void appendAtomInline(std::string& buf, const Token& t) const {
				switch (t.getType()) {
				case Type::BracketGroup:
					appendGroupInline(buf, t);
					break;
				case Type::String:
					buf += '"';
					buf += sv(t);
					buf += '"';
					break;
				case Type::Char:
					buf += '\'';
					buf += sv(t);
					buf += '\'';
					break;
				case Type::FormatString:
					appendVerbatim(buf, t);
					break;
				default:
					buf += sv(t);
					break;
				}
			}

			/**
			 * Renders @p tokens inline into @p buf for measurement. @p prev is the token
			 * emitted just before the span (if any), so that the leading space and unary
			 * operators are accounted for exactly as emitTokens would render them.
			 */
			void appendInline(
				std::string& buf, std::span<const Token> tokens, const Token* prev = nullptr
			) const {
				bool suppress_space = false;
				for (const auto& t: tokens) {
					if (isSkippable(t)) continue;
					const bool leading = suppress_space ? false : needSpace(prev, t);
					suppress_space     = false;
					if (leading) buf += ' ';
					const bool unary = isSignOperator(t) && isPrefixContext(prev);
					appendAtomInline(buf, t);
					if (unary) suppress_space = true;
					prev = &t;
				}
			}

			void appendGroupInline(std::string& buf, const Token& t) const {
				appendUtf8(buf, static_cast<char32_t>(t.getBracketType()));
				appendInline(buf, t.getRecursive());
				appendUtf8(buf, closingBracket(t.getBracketType()));
			}

			/** First token of @p tokens that carries source text, or nullptr. */
			static const Token* firstSignificant(std::span<const Token> tokens) {
				for (const auto& t: tokens)
					if (!isSkippable(t)) return &t;
				return nullptr;
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
					    && needSpace(prev, t))
						points.push_back(i);
					prev = &t;
				}
				return points;
			}

			/* --- wrap-aware rendering into the output --- */

			/**
			 * @brief Renders a bracket group token.
			 *
			 * A `{...}` code block is delegated to emitBlockCurly. Other groups render inline when
			 * they fit within the configured line length; an over-long breakable group is exploded
			 * onto one element per line.
			 */
			void emitGroup(const Token& t) {
				if (isBlockCurly(t)) {
					emitBlockCurly(t);
					return;
				}

				std::string inlined;
				appendGroupInline(inlined, t);

				if (!isBreakable(t) || currentColumn() + inlined.size() <= config.max_line_length) {
					out += inlined;
					return;
				}

				emitGroupExploded(t);
			}

			/**
			 * @brief Explodes a breakable group: opening bracket, one element per indented line,
			 * then the closing bracket at the current indentation. Adds no tokens, so the output
			 * re-tokenizes to the same stream.
			 */
			void emitGroupExploded(const Token& t) {
				appendUtf8(out, static_cast<char32_t>(t.getBracketType()));
				out += '\n';
				indent++;
				for (const auto& element: splitElements(t.getRecursive())) {
					writeIndent();
					emitRun(element.tokens, nullptr);
					if (element.trailing_comma) out += ',';
					out += '\n';
				}
				indent--;
				writeIndent();
				appendUtf8(out, closingBracket(t.getBracketType()));
			}

			/**
			 * @brief Renders a single token.
			 *
			 * Bracket groups recurse through emitGroup; string and char literals are restored with
			 * their delimiters (the lexeme omits them); format strings are reproduced verbatim from
			 * source; any other token is emitted as its raw lexeme.
			 */
			void emitAtom(const Token& t) {
				switch (t.getType()) {
				case Type::BracketGroup:
					emitGroup(t);
					break;
				case Type::String:
					out += '"';
					out += sv(t);
					out += '"';
					break;
				case Type::Char:
					out += '\'';
					out += sv(t);
					out += '\'';
					break;
				case Type::FormatString:
					appendVerbatim(out, t);
					break;
				default:
					out += sv(t);
					break;
				}
			}

			/**
			 * Renders a token sequence inline into the output, recursing through emitGroup so that
			 * nested over-long groups still wrap.
			 *
			 * @param prev The token emitted just before the sequence, for spacing and unary
			 *             operator decisions across the boundary.
			 * @param suppress_leading Whether to omit the space before the first token (used when
			 *                         the sequence starts a fresh line).
			 * @return The last significant token emitted (or @p prev if there was none).
			 */
			const Token* emitTokens(
				std::span<const Token> tokens, const Token* prev, bool suppress_leading = false
			) {
				bool suppress_space = suppress_leading;
				for (const auto& t: tokens) {
					if (isSkippable(t)) continue;
					const bool leading = suppress_space ? false : needSpace(prev, t);
					suppress_space     = false;
					if (leading) out += ' ';
					const bool unary = isSignOperator(t) && isPrefixContext(prev);
					emitAtom(t);
					if (unary) suppress_space = true;
					prev = &t;
				}
				return prev;
			}

			/**
			 * @brief Renders a token run, breaking it across lines when it is over-long.
			 *
			 * A run that fits within the line length is rendered inline. An over-long run is
			 * split at its break points and laid out greedily: each segment that would overflow
			 * the line starts a new line, indented one level deeper than the run itself.
			 * Break points are chosen in order of preference:
			 *   1. method-chain dots (`foo(a).bar(b)` breaks before `.bar`),
			 *   2. spaced binary operators (`a + b` breaks before `+`) — but only when the run
			 *      holds no breakable bracket group, since exploding the group (which emitGroup
			 *      does on its own) yields a better layout than an operator break.
			 *
			 * Only whitespace is introduced, so the output re-tokenizes to the same stream.
			 *
			 * @return The last significant token emitted (or @p prev if there was none).
			 */
			const Token* emitRun(std::span<const Token> tokens, const Token* prev) {
				if (firstSignificant(tokens) == nullptr) return prev;

				std::string inlined;
				appendInline(inlined, tokens, prev);
				if (currentColumn() + inlined.size() <= config.max_line_length)
					return emitTokens(tokens, prev);

				auto breaks = chainBreakPoints(tokens);
				if (breaks.empty()) {
					bool has_breakable_group = false;
					for (const auto& t: tokens)
						if (isBreakable(t)) {
							has_breakable_group = true;
							break;
						}
					if (!has_breakable_group) breaks = operatorBreakPoints(tokens);
				}
				if (breaks.empty()) return emitTokens(tokens, prev);

				breaks.push_back(tokens.size());
				bool  broke         = false;
				bool  first_segment = true;
				usize start         = 0;
				for (const usize end: breaks) {
					if (end == start) continue;
					const auto segment = tokens.subspan(start, end - start);
					start              = end;
					if (firstSignificant(segment) == nullptr) continue;

					std::string measured;
					appendInline(measured, segment, prev);
					if (first_segment
					    || currentColumn() + measured.size() <= config.max_line_length) {
						prev = emitTokens(segment, prev);
					} else {
						if (!broke) {
							indent++;
							broke = true;
						}
						out += '\n';
						writeIndent();
						prev = emitTokens(segment, prev, /*suppress_leading=*/true);
					}
					first_segment = false;
				}
				if (broke) indent--;
				return prev;
			}

			/**
			 * @brief Renders a line comment, re-flowing it onto multiple `#` lines when over-long.
			 *
			 * A comment that fits is reproduced verbatim. An over-long comment is split at word
			 * boundaries; every continuation line repeats the comment prefix (`#`, `##`, ...) at
			 * the current indentation. A single word longer than the line length is kept intact.
			 * This is the one transformation that changes the token stream: one comment token
			 * becomes several with the same combined text.
			 */
			void emitLineComment(const Token& t) {
				const std::string_view text = sv(t);
				if (currentColumn() + text.size() <= config.max_line_length) {
					out += text;
					return;
				}

				usize prefix_end = 0;
				while (prefix_end < text.size() && text[prefix_end] == '#') prefix_end++;
				const std::string prefix(text.substr(0, prefix_end));

				out += prefix;
				bool  line_has_word = false;
				usize pos           = prefix_end;
				while (pos < text.size()) {
					while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) pos++;
					if (pos >= text.size()) break;
					usize end = pos;
					while (end < text.size() && text[end] != ' ' && text[end] != '\t') end++;
					const auto word = text.substr(pos, end - pos);
					pos             = end;

					if (line_has_word
					    && currentColumn() + 1 + word.size() > config.max_line_length) {
						out += '\n';
						writeIndent();
						out += prefix;
					}
					out += ' ';
					out += word;
					line_has_word = true;
				}
			}

			/**
			 * @brief Renders a `{...}` code block across multiple lines.
			 *
			 * Emits the opening brace, then each inner statement on its own line indented one level
			 * deeper, then the closing brace at the current indentation. A block whose body is
			 * empty collapses to `{}` on a single line.
			 */
			void emitBlockCurly(const Token& t) {
				out += '{';
				const Tokens& body = t.getRecursive();

				bool has_content = false;
				for (const auto& c: body) {
					if (!isSkippable(c)) {
						has_content = true;
						break;
					}
				}
				if (!has_content) {
					out += '}';
					return;
				}

				out += '\n';
				indent++;
				emitBlockBody(body);
				indent--;
				writeIndent();
				out += '}';
			}

			/**
			 * Renders a token sequence as a list of statements, one per line. Empty source lines
			 * between statements are preserved up to config.max_empty_lines; leading and trailing
			 * ones are always dropped.
			 */
			void emitBlockBody(const Tokens& tokens) {
				usize        i         = 0;
				const auto   n         = tokens.size();
				const Token* prev_last = nullptr;
				while (i < n) {
					if (isSkippable(tokens[i])) {
						i++;
						continue;
					}
					if (prev_last != nullptr) {
						const usize empty = emptyLinesBetween(*prev_last, tokens[i]);
						out.append(std::min<usize>(empty, config.max_empty_lines), '\n');
					}
					writeIndent();
					const usize next = emitStatement(tokens, i);
					for (usize j = next; j > i; j--)
						if (!isSkippable(tokens[j - 1])) {
							prev_last = &tokens[j - 1];
							break;
						}
					i = next;
					out += '\n';
				}
			}

			/**
			 * @brief Emits one statement from a statement-list context.
			 *
			 * Renders tokens inline starting at @p i until the statement ends. A statement ends at
			 * a `;` terminator, at a trailing line comment (which must own its line), or after a
			 * `{...}` code block — except that a block followed by `else` continues the same
			 * statement, so `if/else if/else` chains stay on one logical statement.
			 *
			 * Expression runs between those boundaries are rendered through emitRun, so an
			 * over-long expression wraps at chain dots or binary operators.
			 *
			 * @param tokens The enclosing statement list.
			 * @param i Index of the first token of the statement.
			 * @return The index of the first token after the emitted statement.
			 */
			usize emitStatement(const Tokens& tokens, usize i) {
				const auto   n         = tokens.size();
				const Token* prev      = nullptr;
				usize        run_start = i;
				bool         at_start  = true;

				const auto flush_run = [&](usize end) {
					prev = emitRun({ tokens.data() + run_start, tokens.data() + end }, prev);
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
						out += ';';
						return consumeTrailingComment(tokens, i);
					}

					// A line comment consumes the rest of its source line, so it must end the line.
					if (isLineComment(t)) {
						flush_run(i);
						if (needSpace(prev, t)) out += ' ';
						emitLineComment(t);
						return i + 1;
					}

					// A code block ends the statement (unless an `else` clause follows).
					if (isBracketGroup(t) && t.getBracketType() == Bracket::Curly
					    && isBlockCurly(t)) {
						flush_run(i);
						// Separate the brace from a preceding token (`fun f() = {`), but not when
						// the block opens the statement (the indentation already positions it).
						if (prev != nullptr) out += ' ';
						emitBlockCurly(t);

						usize j = i + 1;
						while (j < n && isSkippable(tokens[j])) j++;

						if (j < n && isKeyword(tokens[j]) && isStr(tokens[j], "else")) {
							prev      = &t;
							run_start = j;
							i         = j;
							continue;
						}
						if (j < n && isSpecial(tokens[j]) && isStr(tokens[j], ";")) {
							out += ';';
							return consumeTrailingComment(tokens, j);
						}
						return j;
					}

					i++;
				}
				flush_run(n);
				return n;
			}

			/**
			 * After a `;` emitted for the token at @p i, also emits a line comment that trails it
			 * on the same source line (`x = 1; # note` keeps the note on the line).
			 *
			 * @return The index of the first token after the statement (and its trailing comment).
			 */
			usize consumeTrailingComment(const Tokens& tokens, usize i) {
				const auto n = tokens.size();
				usize      j = i + 1;
				while (j < n && isSkippable(tokens[j])) j++;
				if (j < n && isLineComment(tokens[j]) && onSameSourceLine(tokens[i], tokens[j])) {
					out += ' ';
					emitLineComment(tokens[j]);
					return j + 1;
				}
				return i + 1;
			}
		};
	}

	std::string formatTokens(
		const lexer::TokenData& tokens, const FormatConfig& config, std::string_view source
	) {
		return Emitter(config, source).run(tokens.tokens);
	}
}
