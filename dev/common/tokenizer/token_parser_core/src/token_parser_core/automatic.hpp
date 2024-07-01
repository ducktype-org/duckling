/**
 * @file automatic.hpp
 * @brief Useful parsing abstractions for ParserState
 *
 * ParseOne - has four modes depending on the type of second argument:
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
 * ParseAll takes the state and any number of additional arguments and calls parseOne on those
 * arguments from left to right. Additionally it makes the first parsed thing non-ignorable and the
 * rest ignorable so that infinite parsing loops are very unlikely.
 *
 * NullAwareDprint is a wrapper for element specific debug prints called on pointers that prints
 * null if the pointer is null
 *
 * @note ParseOne/ParseAll should be changed to be methods of `tpc::ParserState`
 */
#pragma once

#include "parser_state.hpp"
#include "common_elements.hpp"
#include <rift_definitions/key_spec_op.hpp>

#include "base_element.hpp"
#include <diagnostic/source_position.hpp>

#include <concepts>

#include "parser_ref.hpp"

namespace tpc {
	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	// Useful for debugging:
	//
	// parses one of the available types
	// template<class T>
	// void parseOne([[maybe_unused]]ParserState& state, [[maybe_unused]]T t) {
	// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
	// }

	struct KeywordWrapper {
		Keyword                              what;
		base::Optional<dia::SourcePosition>& pos;
	};

	/**
	 * @brief Parses the expected keyword. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param key The expected keyword.
	 * @param ignorable True if the token is not skipped on error.
	 * @param ignorable False if the token is skipped on error.
	 */
	void parseOne(ParserState& state, Keyword key, bool ignorable = false);

	/**
	 * @brief Parses the expected keyword. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param key The expected keyword.
	 * @param ignorable True if the token is not skipped on error.
	 * @param ignorable False if the token is skipped on error.
	 */
	void parseOne(ParserState& state, KeywordWrapper key, bool ignorable = false);

	/**
	 * @brief Parses the expected Special token. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param spec The expected special token.
	 * @param ignorable True if the token is not skipped on error.
	 * @param ignorable False if the token is skipped on error.
	 */
	void parseOne(ParserState& state, Special spec, bool ignorable = false);

	/**
	 * @brief Parses the expected operator. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param op The expected operator.
	 * @param ignorable True if the token is not skipped on error.
	 * @param ignorable False if the token is skipped on error.
	 */
	void parseOne(ParserState& state, Operator op, bool ignorable = false);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 * @param ignorable True if the token is not skipped on error.
	 * @param ignorable False if the token is skipped on error.
	 */
	void parseOne(ParserState& state, Identifier* result, bool ignorable = false);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 * @param ignorable Ignored.
	 */
	void parseOne(ParserState& state, OptionalIdentifier* result, bool ignorable = false);

	/**
	 * @brief Parses an Element. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed element.
	 * @param ignorable Ignored.
	 */
	template<typename State, std::derived_from<Element> T>
	void parseOne(State& state, ParserRef<T>* result, bool = false) {
		*result = T::parse(state);
	}

	namespace detail {
		// parses all the given elements
		template<typename State, typename T>
		void parseAllInternal(State& state, T t) {
			parseOne(state, t, true);
		}

		template<typename State, typename T, typename... Q>
		void parseAllInternal(State& state, T t, Q... q) {
			parseOne(state, t, true);
			parseAllInternal(state, q...);
		}
	}

	// parses all the given elements
	template<typename State, typename T>
	void parseAll(State& state, T t) {
		parseOne(state, t);
	}

	template<typename State, typename T, typename... Q>
	void parseAll(State& state, T t, Q... q) {
		parseOne(state, t);
		detail::parseAllInternal(state, q...);
	}

	void nullAwareDprint(Identifier, std::ostream& out);
	void nullAwareDprint(OptionalIdentifier, std::ostream& out);

	template<typename T>
	void nullAwareDprint(const ParserRef<T>& ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->dprint(out);
	}
}
