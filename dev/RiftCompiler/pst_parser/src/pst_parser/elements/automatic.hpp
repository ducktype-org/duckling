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

	using MaybeToken = base::Optional<base::c_borrow_ptr<Token>>;

	/**
	 * @brief Parses the expected keyword. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param key The expected keyword.
	 */
	MaybeToken parseOne(ParserState& state, Keyword key, bool ignorable = false);

	/**
	 * @brief Parses the expected Special token. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param spec The expected special token.
	 */
	MaybeToken parseOne(ParserState& state, Special spec, bool ignorable = false);

	/**
	 * @brief Parses the expected operator. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param op The expected operator.
	 */
	MaybeToken parseOne(ParserState& state, Operator op, bool ignorable = false);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 */
	MaybeToken parseOne(ParserState& state, Identifier* result, bool ignorable = false);

	/**
	 * @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed identifier.
	 */
	MaybeToken  parseOne(ParserState& state, OptionalIdentifier* result, bool ignorable = false);

	/**
	 * @brief Parses an Element. Skips on success, logs error on failure.
	 * @param state The current ParserState.
	 * @param result The place to store the parsed element.
	 */
	template<typename State, std::derived_from<Element> T>
	MaybeToken parseOne(State& state, ParserRef<T>* result, bool = false) {
		*result = T::parse(state);
		return {};
	}

	namespace detail {
		// parses all the given elements
		template<typename State, typename It, typename T>
		void parseRest(State& state, It it, T t) {
			*it = parseOne(state, t, true);
		}

		template<typename State, typename It, typename T, typename... Q>
		void parseRest(State& state, It it, T t, Q... q) {
			*it = parseOne(state, t, true);
			it++;
			parseRest(state, it, q...);
		}

		// parses all the given elements
		template<typename State, typename It, typename T>
		void parseFirst(State& state, It it, T t) {
			*it = parseOne(state, t);
			it++;
		}

		template<typename State, typename It, typename T, typename... Q>
		void parseFirst(State& state, It it, T t, Q... q) {
			*it =  parseOne(state, t);
			it++;
			parseRest(state, it, q...);
		}
	}
	template<typename State, typename T, typename... Ts>
	auto parseAll(State& state, T, Ts... args) -> std::array<MaybeToken, sizeof...(Ts) + 1> {
		std::array<MaybeToken, sizeof...(Ts) + 1> res;
		detail::parseFirst(state, res.begin(), args...);
		return res;
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
