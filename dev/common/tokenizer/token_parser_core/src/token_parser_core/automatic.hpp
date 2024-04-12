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
 * ParseAll takes the state and any number of additional arguments and calls parseOne on those
 * arguments from left to right.
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

	/**
	 * @brief Parses the expected keyword. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param key The expected keyword.
	 */
	void parseOne(ParserState& state, Keyword key);

	/**
	 * @brief Parses the expected Special token. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param spec The expected special token.
	 */
	void parseOne(ParserState& state, Special spec);

	/**
	 * @brief Parses the expected operator. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param op The expected operator.
	 */
	void parseOne(ParserState& state, Operator op);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 */
	void parseOne(ParserState& state, Identifier* result);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 */
	void parseOne(ParserState& state, OptionalIdentifier* result);

	/**
	 * @brief Parses an Element. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed element.
	 */
	template<typename State, std::derived_from<Element> T>
	void parseOne(State& state, ParserRef<T>* result) {
		*result = T::parse(state);
	}

	// parses all the given elements
	template<typename State, typename T>
	void parseAll(State& state, T t) {
		parseOne(state, t);
	}

	template<typename State, typename T, typename... Q>
	void parseAll(State& state, T t, Q... q) {
		parseOne(state, t);
		parseAll(state, q...);
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
