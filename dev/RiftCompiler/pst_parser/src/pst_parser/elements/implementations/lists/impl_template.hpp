#pragma once

#include "preamble.hpp"

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

		template<
			class ListElements,
			typename Self,
			bool                      NON_EMPTY,
			lexer::Token::BracketType BRACKETS,
			StateCondition            isSeparator,
			StateCondition            isEnding,
			GetName                   getName>
		static auto parseList(RiftParserState& state) -> ParserRef<Self> {
			auto position = state.getPosition();

			auto out = tpc::makeRef<Self>(position);

			// Handle opening brackets:
			if constexpr (BRACKETS != lexer::Token::BracketType::None) {
				if (!state[0].isBracketGroup(BRACKETS)) {
					state.log(base::make_unique<OpeningBracketMissingError<getName>>(
						state.getPosition(-1), BRACKETS
					));
					return nullptr;
				}
				state.parse(out).goDown();
			}

			usize expr_length{};
			if (state.empty() || isEnding(state, 0)) {
				// Handle empty expression
				if constexpr (NON_EMPTY)
					state.log(base::make_unique<EmptyListError<getName>>(state.getPosition(-1)));
			} else {
				while (true) {
					expr_length = 0;

					// Find next separator or end
					while (!state[(i64) expr_length].is(lexer::Token::Type::Sentinel)
					       && !isSeparator(state, (i64) expr_length)
					       && !isEnding(state, (i64) expr_length)) {
						expr_length++;
					}
					if (expr_length == 0) {
						// Handle empty field errors with sensible ranges
						if (state.empty() || isEnding(state, 0)) {
							auto pos = state.getPosition(-1);
							if (!state.isEOF()) {
								auto other = state.getPosition();
								pos        = dia::SourcePosition(pos, other.getStart());
							}
							state.log(base::make_unique<EmptyFieldError<getName>>(pos));
							break;
						} else {
							state.log(base::make_unique<EmptyFieldError<getName>>(
								state.getPosition(-1, 0)
							));
							state.parse(out).eatOne();
							continue;
						}
					}

					ParserRef<ListElements> ref;
					// @TODO: This is a "temporary" fix.
					// Hopefully we can handle this with a uniform `parse` function for all
					// elements, maybe by polymorphism.
					if constexpr (std::derived_from<ListElements, Expr>) {
						state.parse(out).template with<ListElements>(
							&ref, ListElements::parse, (usize) expr_length, true, false
						);
					} else {
						state.parse(out).template with<ListElements>(&ref, ListElements::parse);
					}
					out->elements.emplace_back(std::move(ref));

					if (isEnding(state, 0)) break;
					if (isSeparator(state, 0))
						state.parse(out).eatOne();
					else
						state.log(base::make_unique<NoSeparatorError<getName>>(state.getPosition())
						);
				}
			}

			// Handle closing brackets
			if constexpr (BRACKETS != lexer::Token::BracketType::None)
				state.parse(out).goUpAndSkip();

			return out;
		}
	};
}
