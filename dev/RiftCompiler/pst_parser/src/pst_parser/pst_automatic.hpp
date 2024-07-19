/**
 * @file pst_automatic.hpp
 * @brief Useful parsing abstractions for RiftParserState
 *
 * one() - has four modes depending on the type of the first argument:
 *  - for Specials, Keywords and Operators from `rift_def` it ensures that the next token has that
 * value and skips it, otherwise it logs an error
 *  - for Identifier* it ensures the next token is an identifier and parses it to the specified
 * location and skips it, otherwise it logs an error
 *  - for OptionalIdentifier* it parses an identifier into the specified location and skips. If
 * There is no identifier next it doesn't do anything
 *  - for ParserRef<T>* it calls the parser of T object into the specified location
 *
 * The optional argument ignorable additionally allows to control behaviour in case of error.
 * If it's set to true then simple parse-able entities(not parser ref) will not be skipped on error.
 * It works as a kind of assumption that is something simple doesn't fit then it's missing not
 * wrong.
 *
 * all() takes the state and any number of additional arguments and calls parseOne on those
 * arguments from left to right. Additionally it makes the first parsed thing non-ignorable and the
 * rest ignorable so that infinite parsing loops are very unlikely.
 */
#pragma once

#include <token_parser_core/automatic.hpp>
#include "rift_parser_element.hpp"

namespace pst {
	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	template<typename State>
	class PSTAutomatic {
	protected:
		State&                            state;
		ParserBorrowRef<pst::RiftElement> el;

	public:
		PSTAutomatic(State& state, ParserBorrowRef<pst::RiftElement> caller):
			  state(state),
			  el(std::move(caller)) {}

		PSTAutomatic(const PSTAutomatic&) = delete;

		// Useful for debugging:
		//
		// parses one of the available types
		// template<class T>
		// void one([[maybe_unused]]T t, [[maybe_unused]]bool = false) {
		// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
		// }

		/**
		 * @brief Parses the expected keyword. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param key The expected keyword.
		 */
		void one(Keyword key, bool ignorable = false) {
			if (!state.tryEat(key)) {
				state.log(base::make_unique<tpc::BadKeywordError>(state.getPosition(), key));
				if (!ignorable) state.tokens().next();
			} else {
				el->addToken(state[-1]);
			}
		}

		/**
		 * @brief Parses the expected Special token. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param spec The expected special token.
		 */
		void one(Special spec, bool ignorable = false) {
			if (!state.tryEat(spec)) {
				state.log(base::make_unique<tpc::BadSpecialError>(state.getPosition(), spec));
				if (!ignorable) state.tokens().next();
			} else {
				el->addToken(state[-1]);
			}
		}

		/**
		 * @brief Parses the expected operator. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param op The expected operator.
		 */
		void one(Operator op, bool ignorable = false) {
			if (!state.tryEat(op)) {
				state.log(base::make_unique<tpc::BadOperatorError>(state.getPosition(), op));
				if (!ignorable) state.tokens().next();
			} else {
				el->addToken(state[-1]);
			}
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param result The place to store the parsed identifier.
		 */
		void one(tpc::Identifier* result, bool ignorable = false) {
			if (!state.ctokens().peek().isIdentifier()) {
				state.log(base::make_unique<tpc::NoIdentifierError>(state.getPosition()));
				result->value = base::StrId("<error>");
				if (!ignorable) state.tokens().next();
				return;
			}
			el->addToken(state[0]);
			result->value = state.tokens().next().getValue();
		}

		/**
		 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
		 * @param state The current ParserState.
		 * @param result The place to store the parsed identifier.
		 */
		void one(tpc::OptionalIdentifier* result, bool = false) {
			if (state.ctokens().peek().isIdentifier()) {
				el->addToken(state[0]);
				result->value = state.tokens().next().getValue();
			}
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<RiftElement> T>
		void one(ParserRef<T>* result, bool = false) {
			with(result, T::parse);
		}

		/**
		 * @brief Parses an Element. Skips on success, logs error on failure.
		 * @param state The current ParserState.
		 * @param result The place to store the parsed element.
		 */
		template<std::derived_from<RiftElement> T>
		void one(base::Optional<ParserRef<T>>* result, bool = false) {
			*result = T::parse(state);
		}

		/**
		 * @brief Call a custom parse function with automation.
		 *
		 * The return type of the parsed function usually has to be specified with the first
		 * template argument.
		 *
		 * @param sink Place to store the new value(works with optionals).
		 * @param fun Parsing function.
		 * @param args Arguments passed to the parsing function
		 */
		template<std::derived_from<RiftElement> El, typename Sink, typename... Args>
		void with(Sink* sink, ParserRef<El> fun(State&, Args...), Args&&... args) {
			*sink = fun(state, std::forward<Args>(args)...);
			el->addChild(*sink);
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		template<typename T>
		bool tryEat(T type) {
			if (state[0].is(type)) {
				el->addToken(state.tokens().next());
				return true;
			}
			return false;
		}

		/**
		 * @brief Eats any token
		 */
		void eatOne() {
			if (state.notEmpty()) el->addToken(state.tokens().next());
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		void goDown() {
			el->addToken(state[0].getSentinelBegin());
			state.goDown();
		}

		/**
		 * @brief Automatic version of the ParserState function
		 */
		void goUpAndSkip() {
			state.goUp();
			el->addToken(state[0].getSentinelEnd());
			state.tokens().skip();
		}

		// parses all the given elements
		template<typename T>
		void all(T t) {
			one(t);
		}

		template<typename T, typename... Q>
		void all(T t, Q... q) {
			one(t);
			parseRest(q...);
		}

	private:
		// parses all the given elements
		template<typename T>
		void parseRest(T t) {
			one(t, true);
		}

		template<typename T, typename... Q>
		void parseRest(T t, Q... q) {
			one(t, true);
			parseRest(q...);
		}
	};
}
