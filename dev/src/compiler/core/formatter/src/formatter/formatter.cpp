#include "formatter.hpp"

#include <base/types/ints.hpp>

#include <lexer/token.hpp>

#include <string>
#include <string_view>

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

		bool isLineComment(const Token& t) { return isComment(t) && sv(t).starts_with("//"); }

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
			return isStr(t, "-") || isStr(t, "+") || isStr(t, "!") || isStr(t, "~");
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

				// Member access binds tightly on both sides: `a.b`.
				if (isOperator(cur) && isStr(cur, ".")) return false;
				if (isOperator(*prev) && isStr(*prev, ".")) return false;

				// Attribute sigil binds to its name: `@Attr`.
				if (isSpecial(*prev) && isStr(*prev, "@")) return false;

				// Type/label colon: no space before, space after (handled by default).
				if (isOperator(cur) && isStr(cur, ":")) return false;

				// Call and index groups bind to the preceding value: `foo(x)`, `arr[i]`.
				if (isBracketGroup(cur)) {
					const Bracket b = cur.getBracketType();
					if (b == Bracket::Round || b == Bracket::Square) {
						if (isKeyword(*prev)) return true;       // `if (...)`, `while (...)`
						if (isValueCloser(*prev)) return false;  // `foo(...)`, `a[...]`
					}
					return true;
				}

				if (!config.space_around_operators && (isOperator(*prev) || isOperator(cur)))
					return false;

				return true;
			}

			/**
			 * Emits a token exactly as it appears in the original source, falling back to its
			 * lexeme when no source span is available.
			 */
			void emitVerbatim(const Token& t) {
				const auto position = t.getPosition();
				const auto start    = position.getStart();
				const auto end      = position.getEnd();
				if (!source.empty() && start <= end && end < source.size())
					out += source.substr(start, end - start + 1);
				else
					out += sv(t);
			}

			/**
			 * @brief Renders a bracket group token.
			 *
			 * A `{...}` code block is delegated to emitBlockCurly; every other group
			 * (`(...)`, `[...]`, an inline `{...}` literal) is rendered on the current line as
			 * its opening bracket, its inline contents, and its closing bracket.
			 */
			void emitGroup(const Token& t) {
				if (isBlockCurly(t)) {
					emitBlockCurly(t);
					return;
				}
				appendUtf8(out, static_cast<char32_t>(t.getBracketType()));
				emitInline(t.getRecursive());
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
				// String/Char lexemes are stored without their delimiters; restore them so the
				// result re-tokenizes to the same literal rather than an identifier.
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
				// Format strings (`f"... {expr} ..."`) cannot be rebuilt from a single lexeme;
				// reproduce them verbatim from the original source span.
				case Type::FormatString:
					emitVerbatim(t);
					break;
				default:
					out += sv(t);
					break;
				}
			}

			/**
			 * Renders a token sequence as a single inline expression (no statement breaks).
			 */
			void emitInline(const Tokens& tokens) {
				const Token* prev           = nullptr;
				bool         suppress_space = false;

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

			/** Renders a token sequence as a list of statements, one per line. */
			void emitBlockBody(const Tokens& tokens) {
				usize      i = 0;
				const auto n = tokens.size();
				while (i < n) {
					if (isSkippable(tokens[i])) {
						i++;
						continue;
					}
					writeIndent();
					i = emitStatement(tokens, i);
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
			 * @param tokens The enclosing statement list.
			 * @param i Index of the first token of the statement.
			 * @return The index of the first token after the emitted statement.
			 */
			usize emitStatement(const Tokens& tokens, usize i) {
				const auto   n              = tokens.size();
				const Token* prev           = nullptr;
				bool         suppress_space = false;

				while (i < n) {
					const Token& t = tokens[i];
					if (isSkippable(t)) {
						i++;
						continue;
					}

					// Statement terminator.
					if (isSpecial(t) && isStr(t, ";")) {
						out += ';';
						return i + 1;
					}

					// A line comment consumes the rest of its source line, so it must end the line.
					if (isLineComment(t)) {
						if (!suppress_space && needSpace(prev, t)) out += ' ';
						out += sv(t);
						return i + 1;
					}

					// A code block ends the statement (unless an `else` clause follows).
					if (isBracketGroup(t) && t.getBracketType() == Bracket::Curly
					    && isBlockCurly(t)) {
						// Separate the brace from a preceding token (`fun f() = {`), but not when
						// the block opens the statement (the indentation already positions it).
						if (prev != nullptr) out += ' ';
						emitBlockCurly(t);

						usize j = i + 1;
						while (j < n && isSkippable(tokens[j])) j++;

						if (j < n && isKeyword(tokens[j]) && isStr(tokens[j], "else")) {
							prev           = &t;
							suppress_space = false;
							i              = j;
							continue;
						}
						if (j < n && isSpecial(tokens[j]) && isStr(tokens[j], ";")) {
							out += ';';
							return j + 1;
						}
						return j;
					}

					const bool leading = suppress_space ? false : needSpace(prev, t);
					suppress_space     = false;
					if (leading) out += ' ';

					const bool unary = isSignOperator(t) && isPrefixContext(prev);
					emitAtom(t);
					if (unary) suppress_space = true;

					prev = &t;
					i++;
				}
				return i;
			}
		};
	}

	std::string formatTokens(
		const lexer::TokenData& tokens, const FormatConfig& config, std::string_view source
	) {
		return Emitter(config, source).run(tokens.tokens);
	}
}
