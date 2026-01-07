#pragma once

#include "preamble.hpp"  // IWYU pragma: keep

#include <unicode/unistr.h>

namespace pst {
	template<GetName type>
	class OpeningBracketMissingError final: public dia::Error {
	private:
		lexer::Token::BracketType bracket;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			std::string str_bracket{};
			icu::UnicodeString(bracket).toUTF8String(str_bracket);
			std::stringstream ss;
			ss << "Opening bracket " << str_bracket << " of a " << type()
			   << " list expected after here.";
			return ss.str();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		OpeningBracketMissingError(dia::SourcePosition pos, lexer::Token::BracketType bracket):
			  dia::Error(pos),
			  bracket(bracket) {}
	};

	template<GetName type>
	class EmptyListError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "This " + type() + " list shouldn't be empty.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyListError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class EmptyListElementError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "This " + type() + " list element shouldn't be empty.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyListElementError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class EmptyFieldError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Empty field in the " + type() + " list.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyFieldError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class NoSeparatorError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return type() + " list separator expected.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoSeparatorError(dia::SourcePosition pos): dia::Error(pos) {}
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
			lexer::Token::BracketType BRACKETS,
			TokenStreamCondition      isSeparator,
			TokenStreamCondition      isEnding,
			GetName                   getName,
			class ParsingClass = ListElements>
		static auto parseList(LangParserState& state) -> MBox<Self> {
			auto position = state.getPosition();

			Box<Self> out = makeBox<Self>(position);

			// Handle opening brackets:
			if constexpr (BRACKETS != lexer::Token::BracketType::None) {
				if (!state[0].isBracketGroup(BRACKETS)) {
					state.log(makeBox<OpeningBracketMissingError<getName>>(
						state.getPosition(-1), BRACKETS
					));
					return nullptr;
				}
				state.parse(out).goDown();
			}

			usize expr_length{};
			if (state.empty() || isEnding(state.ctokens(), 0)) {
				// Handle empty expression
				if constexpr (NON_EMPTY)
					state.log(makeBox<EmptyListError<getName>>(state.getPosition(-1)));
			} else {
				PST_WHILE(true) {
					expr_length = 0;

					PST_WHILE(
						(!isSeparatorOrEnding<isSeparator, isEnding>(state.ctokens(), expr_length))
					) {
						expr_length++;
					}

					state.setFallback(expr_length);

					if (expr_length == 0) {
						// Handle empty field errors with sensible ranges
						if (state.empty() || isEnding(state.ctokens(), 0)) {
							auto pos = state.getPosition(-1);
							if (!state.isEOF()) {
								auto other = state.getPosition();
								pos        = dia::SourcePosition(pos, other.getStart());
							}
							state.log(makeBox<EmptyFieldError<getName>>(pos));
						} else {
							state.log(makeBox<EmptyFieldError<getName>>(state.getPosition(-1, 0)));
						}
					}

					MBox<ListElements> box;
					state.parse(out).template with<ListElements>(&box, ParsingClass::parse);
					if (box.toOpt()) {
						out->elements.emplace_back(nullptr);
						state.parse(out).assign(&out->elements.back(), std::move(box));
					}

					state.exitFallback();

					if (isEnding(state.ctokens(), 0)) break;
					if (isSeparator(state.ctokens(), 0))
						state.parse(out).eatOne();
					else
						state.log(makeBox<NoSeparatorError<getName>>(state.getPosition()));
				}
			}

			// Handle closing brackets
			if constexpr (BRACKETS != lexer::Token::BracketType::None)
				state.parse(out).goUpAndSkip();

			return out;
		}
	};
}
