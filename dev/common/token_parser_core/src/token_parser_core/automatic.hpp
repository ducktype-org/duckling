#pragma once

#include "common_elements.hpp"
#include "parser_state.hpp"
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

	void parseOne(ParserState &state, Keyword key);

	void parseOne(ParserState &state, Special spec);

	void parseOne(ParserState &state, Operator op);

	void parseOne(ParserState &state, Identifier *ident);

	void parseOne(ParserState &state, OptionalIdentifier *ident);

	template<typename State, typename T>
	void parseOne(State &state, ParserRef<T> *t) {
		*t = T::parse(state);
	}

	// parses all the given elements
	template<typename State, typename T>
	void parseAll(State &state, T t) {
		parseOne(state, t);
	}

	template<typename State, class T, class... Q>
	void parseAll(State &state, T t, Q... q) {
		parseOne(state, t);
		parseAll(state, q...);
	}

	void nullAwareDprint(Identifier, std::ostream &out);
	void nullAwareDprint(OptionalIdentifier, std::ostream &out);

	template<class T>
	void nullAwareDprint(const ParserRef<T> &ref, std::ostream &out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->dprint(out);
	}
}
