/**
 * @file automatic.hpp
 * @brief Useful parsing abstractions for ParserState
 * 
 * ParseOne - has four modes depending on the type of second argument:
 *  - for Specials, Keywords and Operators from `rift_def` it ensures that the next token has that value and skips it, otherwise it logs an error
 *  - for Identifier* it ensures the next token is an identifier and parses it to the specified location and skips it, otherwise it logs an error
 *  - for OptionalIdentifier* it parses an identifier into the specified location and skips. If There is no identifier next it doesn't do anything
 *  - for ParserRef<T>* it calls the parser of T object into the specified location 
 * 
 * ParseAll takes the state and any number of additional arguments and calls parseOne on those arguments from left to right.
 * 
 * NullAwareDprint is a wrapper for element specific debug prints called on pointers that prints null if the pointer is null
 * 
 * @note ParseOne/ParseAll should be changed to be methods of `tpc::ParserState`
 */
#pragma once

#include "parser_state.hpp"
#include "common_elements.hpp"
#include <rift_definitions/key_spec_op.hpp>

namespace tpc {
	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	// parses one of the available types
	// template<class T>
	// void parseOne([[maybe_unused]]ParserState& state, [[maybe_unused]]T t) {
	// 	static_assert(sizeof(T) < 0, "parseOne for type `T` is not implemented\n");
	// }

	void parseOne(ParserState& state, Keyword key);

	void parseOne(ParserState& state, Special spec);

	void parseOne(ParserState& state, Operator op);

	void parseOne(ParserState& state, Identifier* ident);

	void parseOne(ParserState& state, OptionalIdentifier* ident);

	template<typename State, typename T>
	void parseOne(State& state, ParserRef<T>* t) {
		*t = T::parse(state);
	}

	// parses all the given elements
	template<typename State, typename T>
	void parseAll(State& state, T t) {
		parseOne(state, t);
	}

	template<typename State, class T, class... Q>
	void parseAll(State& state, T t, Q... q) {
		parseOne(state, t);
		parseAll(state, q...);
	}

	void nullAwareDprint(Identifier, std::ostream& out);
	void nullAwareDprint(OptionalIdentifier, std::ostream& out);

	template<class T>
	void nullAwareDprint(const ParserRef<T>& ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->dprint(out);
	}
}
