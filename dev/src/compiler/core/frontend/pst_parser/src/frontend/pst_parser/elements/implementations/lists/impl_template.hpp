#pragma once

#include "preamble.hpp"  // IWYU pragma: keep

#include <diagnostic/message.hpp>

#include <unicode/unistr.h>

namespace pst {
	template<GetName type>
	class OpeningBracketMissingError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "opening_bracket_missing" };
		}

	public:
		OpeningBracketMissingError(dia::SourcePosition pos, lexer::Token::BracketType bracket):
			  dia::MessageWithCodeFragmentAndCause(pos) {
			std::string s;
			icu::UnicodeString(bracket).toUTF8String(s);
			addArgument<dia::TextArgument>("bracket", s);
			addArgument<dia::TextArgument>("list_type", type());
		}
	};

	template<GetName type>
	class EmptyListError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_list_error" };
		}

	public:
		EmptyListError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("list_type", type());
		}
	};

	template<GetName type>
	class EmptyListElementError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_list_element_error" };
		}

	public:
		EmptyListElementError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("list_type", type());
		}
	};

	template<GetName type>
	class EmptyFieldError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_field_error" };
		}

	public:
		EmptyFieldError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("list_type", type());
		}
	};

	template<GetName type>
	class NoSeparatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_separator_error" };
		}

	public:
		NoSeparatorError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("list_type", type());
		}
	};

	class ListParsingTemplate {
	public:
		ListParsingTemplate() = delete;

		template<TokenStreamCondition isSeparator, TokenStreamCondition isEnding>
		static bool isSeparatorOrEnding(const TokenStream& state, i64 fwd) {
			return isSeparator(state, fwd) || isEnding(state, fwd)
			    || internal::Conditions::isSentinel(state, fwd);
		}

		/**
		 * @brief General Element representing a list of Elements.
		 *
		 * @tparam ListElements - Kept Elements, has to have precise length parse like Expr
		 * @tparam Self - Inheriting class type for construction purposes
		 * @tparam NON_EMPTY - Should empty list be an error.
		 * @tparam ALLOW_TRAILING_SEPARATOR - Should trailing separator be allowed, for example (a,
		 * b,).
		 * @tparam BRACKETS - expected brackets or None if not expected
		 * @tparam isSeparator - Separator should always be skip-able with one skip.
		 * @tparam isEnding - Check for successful ending.
		 * @tparam getName - List name getter for errors.
		 * @tparam ParsingClass - Class that defines parsing for the elements
		 */
		template<
			class ListElements,
			typename Self,
			bool                      NON_EMPTY,
			bool                      ALLOW_TRAILING_SEPARATOR,
			lexer::Token::BracketType BRACKETS,
			TokenStreamCondition      isSeparator,
			TokenStreamCondition      isEnding,
			GetName                   getName,
			class ParsingClass = ListElements>
		static auto parseList(LangParserState& state) -> MBox<Self> {
			Box<Self> out = makeBox<Self>(state);

			// Handle opening brackets:
			if constexpr (BRACKETS != lexer::Token::BracketType::None) {
				if (!state[0].isBracketGroup(BRACKETS)) {
					state.logInt(makeBox<OpeningBracketMissingError<getName>>(
						state.getPosition(-1), BRACKETS
					));
					return nullptr;
				}
				PARSE().goDown();
			}

			usize expr_length{};
			if (state.empty() || isEnding(state.ctokens(), 0)) {
				// Handle empty expression
				if constexpr (NON_EMPTY)
					state.logInt(makeBox<EmptyListError<getName>>(state.getPosition(-1)));
			} else {
				PST_WHILE(true) {
					expr_length = 0;

					PST_WHILE(
						(!isSeparatorOrEnding<isSeparator, isEnding>(state.ctokens(), expr_length))
					) {
						expr_length++;
					}

					PARSE().fallbackLen(expr_length);

					if (expr_length == 0) {
						// Handle empty field errors with sensible ranges
						if (state.empty() || isEnding(state.ctokens(), 0)) {
							auto pos = state.getPosition(-1);
							// Stretch the range to the ending token only if there is one. With
							// nothing left (e.g. `import a,` at the end of the file) the next
							// token is a sentinel past the end of the file.
							if (state.notEmpty()) {
								auto other = state.getPosition();
								pos        = dia::SourcePosition(pos, other.getStart());
							}
							state.logInt(makeBox<EmptyFieldError<getName>>(pos));
						} else {
							state.logInt(makeBox<EmptyFieldError<getName>>(state.getPosition(-1, 0))
							);
						}
					}

					MBox<ListElements> box;
					PARSE().template with<ListElements>(&box, ParsingClass::parse);
					if (box.toOpt()) {
						out->elements.emplace_back(nullptr);
						PARSE().assign(&out->elements.back(), std::move(box));
					}

					PARSE().exitFallback();

					if (isEnding(state.ctokens(), 0)) break;
					if (isSeparator(state.ctokens(), 0)) {
						PARSE().eatOne();
						if (isEnding(state.ctokens(), 0) && ALLOW_TRAILING_SEPARATOR) break;
					} else
						state.logInt(makeBox<NoSeparatorError<getName>>(state.getPosition()));
				}
			}

			// Handle closing brackets
			if constexpr (BRACKETS != lexer::Token::BracketType::None) PARSE().goUpAndSkip();

			PST_RETURN out;
		}
	};
}
