#pragma once

#include "../elements.hpp"

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_ref.hpp>

#include <rift_definitions/key_spec_op.hpp>
#include <rift_definitions/operator_precedence.hpp>
#include <lexer/token.hpp>
#include <lexer/classifications.hpp>

#include <base/exceptions.hpp>
#include <ostream>
#include <variant>
#include <functional>

namespace pst {
	using tpc::makeRef;
	using tpc::parseAll;
	using tpc::parseOne;

	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	using lexer::Token;

	/**
	 * @brief A function which takes in the state of the parser and a distance as its arguments and returns whether a condition is met at the distance from the current position
	 */
	using StateCondition = std::function<bool(const RiftParserState&, usize)>;

	/**
	 * Set up to work on vector like containers with push_back and back
	 */
	template<bool NON_EMPTY, class Container, class Separator>
	bool parseList(RiftParserState& state, Container& cont, Separator sep, StateCondition end_condition) {
		usize expr_length;
		if (state.empty() || end_condition(state, 0)) {
			if (!NON_EMPTY) return true;
			state.fail(-1, "empty list");
			return false;
		} else
			while (true) {
				expr_length = 0;
				while (!state.ctokens().is(Token::Type::Sentinel, expr_length)
				       && !state.ctokens().is(sep, expr_length)
				       && !end_condition(state, expr_length)) {
					expr_length++;
				}
				if (expr_length == 0) {
					if (state.empty()) {
						if (end_condition(state, 0)) break;
						state.fail(-1, "unexpected end to a list");
						return false;
					} else {
						state.fail(-1, "empty field in a list");
						return false;
					}
				}
				cont.emplace_back(Expr::parse(state, expr_length, true));
				if (end_condition(state, 0)) break;
				if (state.ctokens().is(sep))
					state.tokens().skip();
				else
					state.err.logError(state.ctokens().peek().getPosition(), "separator expected");
			}
		return true;
	}
}
