#pragma once

#include "../elements.hpp"

#include <base/exceptions.hpp>
#include <lexer/token.hpp>
#include <ostream>
#include <rift_definitions/key_spec_op.hpp>
#include <rift_definitions/operator_precedence.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <variant>

namespace pst {
	using tpc::makeRef;
	using tpc::parseAll;
	using tpc::parseOne;

	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	using lexer::Token;

	/**
	 * Set up to work on vector like containers with push_back and back
	 */
	template<bool NON_EMPTY, class Container, class Separator, class Ending>
	bool parseList(RiftParserState& state, Container& cont, Separator sep, Ending end) {
		usize expr_length;
		if (state.empty() || state.ctokens().is(end)) {
			if (!NON_EMPTY) return true;
			state.fail(-1, "empty list");
			return false;
		} else
			while (true) {
				expr_length = 0;
				while (!state.ctokens().is(Token::Type::Sentinel, expr_length)
				       && !state.ctokens().is(sep, expr_length)
				       && !state.ctokens().is(end, expr_length)) {
					expr_length++;
				}
				if (expr_length == 0) {
					if (state.empty()) {
						if (end == Token::Type::Sentinel) {
							break;
						}
						state.fail(-1, "unexpected end to a list");
						return false;
					} else {
						state.fail(-1, "empty field in a list");
						return false;
					}
				}
				cont.emplace_back(Expr::parse(state, expr_length, true));
				if (state.ctokens().is(end)) {
					break;
				}
				if (state.ctokens().is(sep)) {
					state.tokens().skip();
				} else {
					state.err.logError(state.ctokens().peek().getPosition(), "separator expected");
				}
			}
		return true;
	}
}
